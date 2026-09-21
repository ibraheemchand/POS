#include "core/commission_service.h"
#include "core/database.h"
#include "core/settings_service.h"

namespace pos {
namespace {
constexpr qint64 kBpScale = 10000; // 100.00% expressed in basis points

qint64 parseBp(const QString& value, qint64 fallback) {
    bool ok = false;
    const auto parsed = value.toLongLong(&ok);
    return ok ? parsed : fallback;
}
} // namespace

CommissionService::CommissionService(std::shared_ptr<Database> database) : db_(std::move(database)) {}

CommissionSettings CommissionService::settings() const {
    SettingsService settings(db_);
    CommissionSettings result;
    result.commissionRateBp = parseBp(settings.value("commission.rate_bp"), result.commissionRateBp);
    result.partnerShareBp = parseBp(settings.value("commission.partner_share_bp"), result.partnerShareBp);
    result.ownerMinShareBp = parseBp(settings.value("commission.owner_min_share_bp"), result.ownerMinShareBp);
    return result;
}

void CommissionService::setSettings(const CommissionSettings& settings) {
    if (settings.commissionRateBp < 0 || settings.commissionRateBp > kBpScale) throw DatabaseError("commission rate must be between 0% and 100%");
    if (settings.partnerShareBp < 0) throw DatabaseError("partner share cannot be negative");
    if (settings.ownerMinShareBp < 0) throw DatabaseError("owner share cannot be negative");
    if (settings.partnerShareBp + settings.ownerMinShareBp > settings.commissionRateBp) throw DatabaseError("partner share plus owner share cannot exceed the total commission rate");
    SettingsService store(db_);
    store.setValue("commission.rate_bp", QString::number(settings.commissionRateBp));
    store.setValue("commission.partner_share_bp", QString::number(settings.partnerShareBp));
    store.setValue("commission.owner_min_share_bp", QString::number(settings.ownerMinShareBp));
}

Money CommissionService::flexibleCap(Money retailAmount, const CommissionSettings& snapshot) const {
    const auto flexibleBp = snapshot.commissionRateBp - snapshot.partnerShareBp - snapshot.ownerMinShareBp;
    if (flexibleBp <= 0 || retailAmount <= 0) return 0;
    return (retailAmount * flexibleBp) / kBpScale;
}

CommissionBreakdown CommissionService::computeBreakdown(Money retailAmount, Money discountAmount, bool overrideApproved, const CommissionSettings& snapshot) const {
    if (retailAmount < 0) throw DatabaseError("retail amount cannot be negative");
    if (discountAmount < 0) throw DatabaseError("discount amount cannot be negative");
    const auto cap = flexibleCap(retailAmount, snapshot);
    const bool overridden = discountAmount > cap;
    if (overridden && !overrideApproved) throw DatabaseError("discount exceeds the allowed flexible margin; a manager override is required");

    CommissionBreakdown breakdown;
    breakdown.retailAmount = retailAmount;
    breakdown.commissionAmount = (retailAmount * snapshot.commissionRateBp) / kBpScale;
    breakdown.partnerAmount = (retailAmount * snapshot.partnerShareBp) / kBpScale;
    breakdown.discountAmount = discountAmount;
    breakdown.ownerAmount = breakdown.commissionAmount - breakdown.partnerAmount - discountAmount;
    breakdown.flexibleCap = cap;
    breakdown.overridden = overridden;
    return breakdown;
}

CommissionTotals CommissionService::totals(const QDate& from, const QDate& to) const {
    if (!from.isValid() || !to.isValid() || from > to) throw DatabaseError("invalid report dates");
    CommissionTotals result;
    auto query = db_->prepare(
        "SELECT COALESCE(SUM(retail_amount_paisa),0), COALESCE(SUM(commission_amount_paisa),0), "
        "COALESCE(SUM(partner_amount_paisa),0), COALESCE(SUM(owner_amount_paisa),0), "
        "COALESCE(SUM(discount_amount_paisa),0), COALESCE(SUM(overridden),0) "
        "FROM sale_item_commissions c JOIN sales s ON s.id=c.sale_id "
        "WHERE s.status!='voided' AND c.created_at>=? AND c.created_at<?");
    query.bind(1, from.toString(Qt::ISODate));
    query.bind(2, to.addDays(1).toString(Qt::ISODate));
    query.stepRow();
    result.revenue = query.integer(0);
    result.commissionPool = query.integer(1);
    result.partnerAccrued = query.integer(2);
    result.ownerProfit = query.integer(3);
    result.discountsGiven = query.integer(4);
    result.overrideCount = query.integer(5);
    return result;
}

QList<CommissionLedgerRow> CommissionService::ledger(const QDate& from, const QDate& to, int limit) const {
    if (!from.isValid() || !to.isValid() || from > to) throw DatabaseError("invalid report dates");
    QList<CommissionLedgerRow> rows;
    auto query = db_->prepare(
        "SELECT s.invoice_no, p.name, c.retail_amount_paisa, c.discount_amount_paisa, c.partner_amount_paisa, c.owner_amount_paisa, c.overridden, c.created_at "
        "FROM sale_item_commissions c "
        "JOIN sales s ON s.id=c.sale_id "
        "JOIN products p ON p.id=c.product_id "
        "WHERE s.status!='voided' AND c.created_at>=? AND c.created_at<? "
        "ORDER BY c.created_at DESC LIMIT ?");
    query.bind(1, from.toString(Qt::ISODate));
    query.bind(2, to.addDays(1).toString(Qt::ISODate));
    query.bind(3, static_cast<qint64>(limit));
    while (query.stepRow()) {
        rows.append({query.text(0), query.text(1), query.integer(2), query.integer(3), query.integer(4), query.integer(5), query.integer(6) != 0, query.text(7)});
    }
    return rows;
}
} // namespace pos
