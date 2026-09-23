#include "core/partner_service.h"
#include "core/database.h"
#include "core/auth_session.h"

namespace pos {
namespace {
constexpr qint64 kBpScale = 10000; // 100.00% in basis points

void validateConfig(const CommissionConfig& config) {
    if (config.totalBp < 0 || config.totalBp > kBpScale) throw DatabaseError("total % must be between 0 and 100");
    if (config.partnerBp < 0 || config.partnerBp > kBpScale) throw DatabaseError("partner % must be between 0 and 100");
    if (config.partnerBp > config.totalBp) throw DatabaseError("partner % cannot be greater than total %");
    if (config.partnerBp > 0 && config.partnerId.trimmed().isEmpty()) throw DatabaseError("select a partner when the partner % is greater than 0");
}

// Applies a signed change to the partner balance and appends a ledger row whose
// running_balance reflects the balance AFTER the change (mirrors customer_ledger).
void postLedger(Database& db, const QString& partnerId, const QString& entryType, const QString& saleId,
                const QString& sourceType, const QString& sourceId, const QString& description,
                Money debit, Money credit) {
    auto update = db.prepare("UPDATE partners SET balance_paisa=balance_paisa+?-? WHERE id=?");
    update.bind(1, credit); update.bind(2, debit); update.bind(3, partnerId); update.execute();
    auto insert = db.prepare(
        "INSERT INTO partner_ledger(id,partner_id,entry_type,sale_id,source_type,source_id,description,debit_paisa,credit_paisa,running_balance_paisa,created_at) "
        "SELECT ?,?,?,?,?,?,?,?,?,balance_paisa,? FROM partners WHERE id=?");
    insert.bind(1, uuid()); insert.bind(2, partnerId); insert.bind(3, entryType);
    if (saleId.isEmpty()) insert.bindNull(4); else insert.bind(4, saleId);
    if (sourceType.isEmpty()) insert.bindNull(5); else insert.bind(5, sourceType);
    if (sourceId.isEmpty()) insert.bindNull(6); else insert.bind(6, sourceId);
    insert.bind(7, description); insert.bind(8, debit); insert.bind(9, credit); insert.bind(10, utcNow()); insert.bind(11, partnerId);
    insert.execute();
}

void insertProfit(Database& db, const QString& saleId, const QString& sourceType, const QString& sourceId,
                  const QString& partnerId, const QString& description, Money value, Money commission,
                  Money partnerAmount, qint64 totalBp, qint64 partnerBp, bool reversal) {
    auto insert = db.prepare(
        "INSERT INTO profit_entries(id,sale_id,source_type,source_id,partner_id,description,sale_value_paisa,commission_paisa,partner_amount_paisa,my_profit_paisa,total_pct_bp,partner_pct_bp,is_reversal,created_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    insert.bind(1, uuid()); insert.bind(2, saleId); insert.bind(3, sourceType); insert.bind(4, sourceId);
    if (partnerId.isEmpty()) insert.bindNull(5); else insert.bind(5, partnerId);
    insert.bind(6, description); insert.bind(7, value); insert.bind(8, commission); insert.bind(9, partnerAmount);
    insert.bind(10, commission - partnerAmount); insert.bind(11, totalBp); insert.bind(12, partnerBp);
    insert.bind(13, reversal ? 1 : 0); insert.bind(14, utcNow());
    insert.execute();
}

void bindRange(Statement& query, int fromIndex, const QDate& from, const QDate& to) {
    if (!from.isValid() || !to.isValid() || from > to) throw DatabaseError("invalid report dates");
    query.bind(fromIndex, from.toString(Qt::ISODate));
    query.bind(fromIndex + 1, to.addDays(1).toString(Qt::ISODate));
}
} // namespace

PartnerService::PartnerService(std::shared_ptr<Database> database) : db_(std::move(database)) {}

// ---------------------------------------------------------------------------
// Cashier-facing (ungated) posting and reversal
// ---------------------------------------------------------------------------

void PartnerService::postSaleCommissions(const QString& saleId) {
    // One entry per course (grouped over its book lines), one per standalone book.
    auto courses = db_->prepare(
        "SELECT si.course_id, b.name, si.total_pct_bp, si.partner_pct_bp, si.partner_id, "
        "SUM(si.line_total_paisa), SUM(si.line_total_paisa*si.total_pct_bp/10000), SUM(si.line_total_paisa*si.partner_pct_bp/10000) "
        "FROM sale_items si LEFT JOIN bundles b ON b.id=si.course_id "
        "WHERE si.sale_id=? AND si.course_id IS NOT NULL AND si.course_id<>'' GROUP BY si.course_id");
    courses.bind(1, saleId);
    while (courses.stepRow()) {
        const auto commission = courses.integer(6);
        if (commission <= 0) continue;
        const auto courseId = courses.text(0);
        const auto partnerId = courses.text(4);
        const auto partnerAmount = courses.integer(7);
        insertProfit(*db_, saleId, "course", courseId, partnerId, "Course: " + courses.text(1),
                     courses.integer(5), commission, partnerAmount, courses.integer(2), courses.integer(3), false);
        if (!partnerId.isEmpty() && partnerAmount > 0)
            postLedger(*db_, partnerId, "sale_credit", saleId, "course", courseId, "Commission: " + courses.text(1), 0, partnerAmount);
    }

    auto books = db_->prepare(
        "SELECT si.id, si.product_id, p.name, si.total_pct_bp, si.partner_pct_bp, si.partner_id, "
        "si.line_total_paisa, si.line_total_paisa*si.total_pct_bp/10000, si.line_total_paisa*si.partner_pct_bp/10000 "
        "FROM sale_items si LEFT JOIN products p ON p.id=si.product_id "
        "WHERE si.sale_id=? AND (si.course_id IS NULL OR si.course_id='')");
    books.bind(1, saleId);
    while (books.stepRow()) {
        const auto commission = books.integer(7);
        if (commission <= 0) continue;
        const auto productId = books.text(1);
        const auto partnerId = books.text(5);
        const auto partnerAmount = books.integer(8);
        insertProfit(*db_, saleId, "book", productId, partnerId, "Book: " + books.text(2),
                     books.integer(6), commission, partnerAmount, books.integer(3), books.integer(4), false);
        if (!partnerId.isEmpty() && partnerAmount > 0)
            postLedger(*db_, partnerId, "sale_credit", saleId, "book", productId, "Commission: " + books.text(2), 0, partnerAmount);
    }
}

void PartnerService::reverseSaleItem(const QString& saleId, const QString& saleItemId, Money returnedAmount, const QString& reason) {
    if (returnedAmount <= 0) return;
    auto item = db_->prepare("SELECT course_id, product_id, total_pct_bp, partner_pct_bp, partner_id FROM sale_items WHERE id=? AND sale_id=?");
    item.bind(1, saleItemId); item.bind(2, saleId);
    if (!item.stepRow()) return;
    const auto courseId = item.text(0);
    const auto productId = item.text(1);
    const auto totalBp = item.integer(2);
    const auto partnerBp = item.integer(3);
    const auto partnerId = item.text(4);
    const auto commission = returnedAmount * totalBp / kBpScale;
    if (commission <= 0) return;
    const auto partnerAmount = returnedAmount * partnerBp / kBpScale;
    const bool isCourse = !courseId.isEmpty();
    const auto sourceType = isCourse ? QString("course") : QString("book");
    const auto sourceId = isCourse ? courseId : productId;
    insertProfit(*db_, saleId, sourceType, sourceId, partnerId, reason,
                 -returnedAmount, -commission, -partnerAmount, totalBp, partnerBp, true);
    if (!partnerId.isEmpty() && partnerAmount > 0)
        postLedger(*db_, partnerId, "reversal", saleId, sourceType, sourceId, reason, partnerAmount, 0);
}

void PartnerService::reverseSale(const QString& saleId, const QString& reason) {
    auto lines = db_->prepare("SELECT id, line_total_paisa FROM sale_items WHERE sale_id=?");
    lines.bind(1, saleId);
    QList<QPair<QString, Money>> items;
    while (lines.stepRow()) items.append({lines.text(0), lines.integer(1)});
    for (const auto& line : items) reverseSaleItem(saleId, line.first, line.second, reason);
}

// ---------------------------------------------------------------------------
// Config resolution (ungated — used to stamp the snapshot at checkout)
// ---------------------------------------------------------------------------

CommissionConfig PartnerService::resolveCourseConfig(const QString& bundleId) const {
    CommissionConfig config;
    if (bundleId.isEmpty()) return config;
    auto query = db_->prepare("SELECT total_pct_bp, partner_pct_bp, partner_id FROM bundles WHERE id=?");
    query.bind(1, bundleId);
    if (query.stepRow()) { config.totalBp = query.integer(0); config.partnerBp = query.integer(1); config.partnerId = query.text(2); }
    return config;
}

CommissionConfig PartnerService::resolveBookConfig(const QString& productId) const {
    CommissionConfig config;
    if (productId.isEmpty()) return config;
    auto query = db_->prepare("SELECT total_pct_bp, partner_pct_bp, partner_id FROM products WHERE id=?");
    query.bind(1, productId);
    if (query.stepRow()) { config.totalBp = query.integer(0); config.partnerBp = query.integer(1); config.partnerId = query.text(2); }
    return config;
}

// ---------------------------------------------------------------------------
// Owner-only (gated) partner management
// ---------------------------------------------------------------------------

QString PartnerService::createPartner(const Partner& partner) {
    AuthSession::instance().requireUnlocked();
    if (partner.name.trimmed().isEmpty()) throw DatabaseError("partner name is required");
    const auto id = uuid();
    auto insert = db_->prepare("INSERT INTO partners(id,name,phone,notes,balance_paisa,is_archived,created_at) VALUES(?,?,?,?,0,0,?)");
    insert.bind(1, id); insert.bind(2, partner.name.trimmed()); insert.bind(3, partner.phone.trimmed());
    insert.bind(4, partner.notes.trimmed()); insert.bind(5, utcNow()); insert.execute();
    return id;
}

void PartnerService::updatePartner(const Partner& partner) {
    AuthSession::instance().requireUnlocked();
    if (partner.name.trimmed().isEmpty()) throw DatabaseError("partner name is required");
    auto update = db_->prepare("UPDATE partners SET name=?,phone=?,notes=? WHERE id=?");
    update.bind(1, partner.name.trimmed()); update.bind(2, partner.phone.trimmed());
    update.bind(3, partner.notes.trimmed()); update.bind(4, partner.id); update.execute();
    if (sqlite3_changes(db_->handle()) != 1) throw DatabaseError("partner not found");
}

void PartnerService::archivePartner(const QString& partnerId) {
    AuthSession::instance().requireUnlocked();
    auto update = db_->prepare("UPDATE partners SET is_archived=1 WHERE id=?");
    update.bind(1, partnerId); update.execute();
    if (sqlite3_changes(db_->handle()) != 1) throw DatabaseError("partner not found");
}

QList<Partner> PartnerService::listPartners(bool includeArchived) const {
    AuthSession::instance().requireUnlocked();
    QList<Partner> partners;
    auto query = db_->prepare(QString("SELECT id,name,phone,notes,balance_paisa,is_archived FROM partners %1 ORDER BY name")
                                  .arg(includeArchived ? "" : "WHERE is_archived=0").toUtf8().constData());
    while (query.stepRow())
        partners.append({query.text(0), query.text(1), query.text(2), query.text(3), query.integer(4), query.integer(5) != 0});
    return partners;
}

// ---------------------------------------------------------------------------
// Owner-only (gated) commission configuration
// ---------------------------------------------------------------------------

CommissionConfig PartnerService::courseConfig(const QString& bundleId) const {
    AuthSession::instance().requireUnlocked();
    return resolveCourseConfig(bundleId);
}

void PartnerService::setCourseConfig(const QString& bundleId, const CommissionConfig& config) {
    AuthSession::instance().requireUnlocked();
    validateConfig(config);
    auto update = db_->prepare("UPDATE bundles SET total_pct_bp=?,partner_pct_bp=?,partner_id=? WHERE id=?");
    update.bind(1, config.totalBp); update.bind(2, config.partnerBp);
    if (config.partnerId.trimmed().isEmpty()) update.bindNull(3); else update.bind(3, config.partnerId);
    update.bind(4, bundleId); update.execute();
    if (sqlite3_changes(db_->handle()) != 1) throw DatabaseError("course not found");
}

CommissionConfig PartnerService::bookConfig(const QString& productId) const {
    AuthSession::instance().requireUnlocked();
    return resolveBookConfig(productId);
}

void PartnerService::setBookConfig(const QString& productId, const CommissionConfig& config) {
    AuthSession::instance().requireUnlocked();
    validateConfig(config);
    auto update = db_->prepare("UPDATE products SET total_pct_bp=?,partner_pct_bp=?,partner_id=? WHERE id=?");
    update.bind(1, config.totalBp); update.bind(2, config.partnerBp);
    if (config.partnerId.trimmed().isEmpty()) update.bindNull(3); else update.bind(3, config.partnerId);
    update.bind(4, productId); update.execute();
    if (sqlite3_changes(db_->handle()) != 1) throw DatabaseError("book not found");
}

QList<CommissionConfigRow> PartnerService::listCommissionConfigs(const QString& searchTerm) const {
    AuthSession::instance().requireUnlocked();
    QList<CommissionConfigRow> rows;
    const auto like = "%" + searchTerm.trimmed() + "%";
    auto courses = db_->prepare(
        "SELECT b.id,b.name,b.grade_label,b.total_pct_bp,b.partner_pct_bp,b.partner_id,p.name "
        "FROM bundles b LEFT JOIN partners p ON p.id=b.partner_id "
        "WHERE b.is_archived=0 AND b.name LIKE ? ORDER BY b.name");
    courses.bind(1, like);
    while (courses.stepRow())
        rows.append({courses.text(0), courses.text(1), "course", courses.text(2), courses.integer(3), courses.integer(4), courses.text(5), courses.text(6)});
    auto books = db_->prepare(
        "SELECT pr.id,pr.name,pr.base_unit,pr.total_pct_bp,pr.partner_pct_bp,pr.partner_id,pa.name "
        "FROM products pr LEFT JOIN partners pa ON pa.id=pr.partner_id "
        "WHERE pr.is_deleted=0 AND pr.name LIKE ? ORDER BY pr.name");
    books.bind(1, like);
    while (books.stepRow())
        rows.append({books.text(0), books.text(1), "book", books.text(2), books.integer(3), books.integer(4), books.text(5), books.text(6)});
    return rows;
}

// ---------------------------------------------------------------------------
// Owner-only (gated) ledger and payouts
// ---------------------------------------------------------------------------

QList<PartnerLedgerRow> PartnerService::partnerLedger(const QString& partnerId, const QDate& from, const QDate& to) const {
    AuthSession::instance().requireUnlocked();
    QList<PartnerLedgerRow> rows;
    auto query = db_->prepare(
        "SELECT l.id,l.entry_type,l.sale_id,s.invoice_no,l.description,l.debit_paisa,l.credit_paisa,l.running_balance_paisa,l.created_at "
        "FROM partner_ledger l LEFT JOIN sales s ON s.id=l.sale_id "
        "WHERE l.partner_id=? AND l.created_at>=? AND l.created_at<? ORDER BY l.created_at");
    query.bind(1, partnerId); bindRange(query, 2, from, to);
    while (query.stepRow())
        rows.append({query.text(0), query.text(1), query.text(2), query.text(3), query.text(4), query.integer(5), query.integer(6), query.integer(7), query.text(8)});
    return rows;
}

void PartnerService::recordPayout(const QString& partnerId, Money amount, const QString& note) {
    AuthSession::instance().requireUnlocked();
    if (amount <= 0) throw DatabaseError("payout amount must be greater than zero");
    Transaction tx(db_->handle());
    auto exists = db_->prepare("SELECT 1 FROM partners WHERE id=?"); exists.bind(1, partnerId);
    if (!exists.stepRow()) throw DatabaseError("partner not found");
    postLedger(*db_, partnerId, "payout", {}, {}, {}, note.trimmed().isEmpty() ? QString("Payout") : note.trimmed(), amount, 0);
    tx.commit();
}

// ---------------------------------------------------------------------------
// Owner-only (gated) reports — all read the saved snapshot rows, net of returns
// ---------------------------------------------------------------------------

QList<ProfitEntryRow> PartnerService::profitEntries(const QDate& from, const QDate& to) const {
    AuthSession::instance().requireUnlocked();
    QList<ProfitEntryRow> rows;
    auto query = db_->prepare(
        "SELECT pe.sale_id,s.invoice_no,pe.source_type,"
        "COALESCE(b.name,p.name,pe.description),COALESCE(pr.name,''),"
        "pe.sale_value_paisa,pe.commission_paisa,pe.partner_amount_paisa,pe.my_profit_paisa,pe.is_reversal,pe.created_at "
        "FROM profit_entries pe "
        "LEFT JOIN sales s ON s.id=pe.sale_id "
        "LEFT JOIN bundles b ON b.id=pe.source_id AND pe.source_type='course' "
        "LEFT JOIN products p ON p.id=pe.source_id AND pe.source_type='book' "
        "LEFT JOIN partners pr ON pr.id=pe.partner_id "
        "WHERE pe.created_at>=? AND pe.created_at<? ORDER BY pe.created_at DESC");
    bindRange(query, 1, from, to);
    while (query.stepRow())
        rows.append({query.text(0), query.text(1), query.text(2), query.text(3), query.text(4),
                     query.integer(5), query.integer(6), query.integer(7), query.integer(8), query.integer(9) != 0, query.text(10)});
    return rows;
}

ProfitSummary PartnerService::summary(const QDate& from, const QDate& to) const {
    AuthSession::instance().requireUnlocked();
    ProfitSummary result;
    auto totals = db_->prepare(
        "SELECT COALESCE(SUM(sale_value_paisa),0),COALESCE(SUM(commission_paisa),0),COALESCE(SUM(partner_amount_paisa),0),COALESCE(SUM(my_profit_paisa),0) "
        "FROM profit_entries WHERE created_at>=? AND created_at<?");
    bindRange(totals, 1, from, to); totals.stepRow();
    result.totalSales = totals.integer(0); result.totalCommission = totals.integer(1);
    result.totalPartner = totals.integer(2); result.myProfit = totals.integer(3);
    auto payouts = db_->prepare("SELECT COALESCE(SUM(debit_paisa),0) FROM partner_ledger WHERE entry_type='payout' AND created_at>=? AND created_at<?");
    bindRange(payouts, 1, from, to); payouts.stepRow(); result.totalPaidOut = payouts.integer(0);
    auto owed = db_->prepare("SELECT COALESCE(SUM(balance_paisa),0) FROM partners"); owed.stepRow(); result.totalOwed = owed.integer(0);
    return result;
}

QList<SourceReportRow> PartnerService::courseReport(const QDate& from, const QDate& to) const {
    AuthSession::instance().requireUnlocked();
    QList<SourceReportRow> rows;
    auto query = db_->prepare(
        "SELECT pe.source_id,COALESCE(b.name,'(deleted course)'),COALESCE(pr.name,'—'),"
        "SUM(CASE WHEN pe.is_reversal=0 THEN 1 ELSE 0 END),"
        "SUM(pe.sale_value_paisa),SUM(pe.commission_paisa),SUM(pe.partner_amount_paisa),SUM(pe.my_profit_paisa) "
        "FROM profit_entries pe LEFT JOIN bundles b ON b.id=pe.source_id LEFT JOIN partners pr ON pr.id=pe.partner_id "
        "WHERE pe.source_type='course' AND pe.created_at>=? AND pe.created_at<? GROUP BY pe.source_id ORDER BY b.name");
    bindRange(query, 1, from, to);
    while (query.stepRow())
        rows.append({query.text(0), query.text(1), query.text(2), query.integer(3), query.integer(4), query.integer(5), query.integer(6), query.integer(7)});
    return rows;
}

QList<SourceReportRow> PartnerService::standaloneBookReport(const QDate& from, const QDate& to) const {
    AuthSession::instance().requireUnlocked();
    QList<SourceReportRow> rows;
    auto query = db_->prepare(
        "SELECT pe.source_id,COALESCE(p.name,'(deleted book)'),COALESCE(pr.name,'—'),"
        "SUM(CASE WHEN pe.is_reversal=0 THEN 1 ELSE 0 END),"
        "SUM(pe.sale_value_paisa),SUM(pe.commission_paisa),SUM(pe.partner_amount_paisa),SUM(pe.my_profit_paisa) "
        "FROM profit_entries pe LEFT JOIN products p ON p.id=pe.source_id LEFT JOIN partners pr ON pr.id=pe.partner_id "
        "WHERE pe.source_type='book' AND pe.created_at>=? AND pe.created_at<? GROUP BY pe.source_id ORDER BY p.name");
    bindRange(query, 1, from, to);
    while (query.stepRow())
        rows.append({query.text(0), query.text(1), query.text(2), query.integer(3), query.integer(4), query.integer(5), query.integer(6), query.integer(7)});
    return rows;
}

QList<PartnerReportRow> PartnerService::partnerReport(const QDate& from, const QDate& to) const {
    AuthSession::instance().requireUnlocked();
    QList<PartnerReportRow> rows;
    auto query = db_->prepare(
        "SELECT pt.id,pt.name,pt.balance_paisa,"
        "COALESCE((SELECT SUM(partner_amount_paisa) FROM profit_entries pe WHERE pe.partner_id=pt.id AND pe.created_at>=?1 AND pe.created_at<?2),0),"
        "COALESCE((SELECT SUM(debit_paisa) FROM partner_ledger l WHERE l.partner_id=pt.id AND l.entry_type='payout' AND l.created_at>=?1 AND l.created_at<?2),0) "
        "FROM partners pt WHERE pt.is_archived=0 ORDER BY pt.name");
    bindRange(query, 1, from, to);
    while (query.stepRow())
        rows.append({query.text(0), query.text(1), query.integer(3), query.integer(4), query.integer(2)});
    return rows;
}

QList<SourceReportRow> PartnerService::partnerBreakdown(const QString& partnerId, const QDate& from, const QDate& to) const {
    AuthSession::instance().requireUnlocked();
    QList<SourceReportRow> rows;
    auto query = db_->prepare(
        "SELECT pe.source_type||':'||pe.source_id,"
        "COALESCE(b.name,p.name,'(deleted)'),'',"
        "SUM(CASE WHEN pe.is_reversal=0 THEN 1 ELSE 0 END),"
        "SUM(pe.sale_value_paisa),SUM(pe.commission_paisa),SUM(pe.partner_amount_paisa),SUM(pe.my_profit_paisa) "
        "FROM profit_entries pe "
        "LEFT JOIN bundles b ON b.id=pe.source_id AND pe.source_type='course' "
        "LEFT JOIN products p ON p.id=pe.source_id AND pe.source_type='book' "
        "WHERE pe.partner_id=? AND pe.created_at>=? AND pe.created_at<? GROUP BY pe.source_type,pe.source_id ORDER BY 2");
    query.bind(1, partnerId); bindRange(query, 2, from, to);
    while (query.stepRow())
        rows.append({query.text(0), query.text(1), query.text(2), query.integer(3), query.integer(4), query.integer(5), query.integer(6), query.integer(7)});
    return rows;
}

} // namespace pos
