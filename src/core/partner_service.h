#pragma once
#include "core/types.h"
#include <memory>
#include <QDate>

namespace pos {
class Database;

// Percentages are stored as basis points (1% = 100 bp), matching the existing
// commission_service so all margin math stays in integers.
struct CommissionConfig {
    qint64 totalBp{};      // share of the sale value counted as commission
    qint64 partnerBp{};    // share of the sale value paid to the linked partner
    QString partnerId;     // empty = no partner (full commission is my profit)
};

struct Partner {
    QString id;
    QString name;
    QString phone;
    QString notes;
    Money balance{}; // positive = still owed to the partner
    bool archived{};
};

struct CommissionConfigRow {
    QString id;
    QString name;
    QString kind;        // "course" or "book"
    QString extra;       // grade label (course) or SKU/unit (book), for display
    qint64 totalBp{};
    qint64 partnerBp{};
    QString partnerId;
    QString partnerName; // resolved, or empty
};

struct PartnerLedgerRow {
    QString id;
    QString entryType;   // sale_credit | payout | reversal
    QString saleId;
    QString invoiceNo;
    QString description;
    Money debit{};
    Money credit{};
    Money balance{};
    QString createdAt;
};

struct ProfitEntryRow {
    QString saleId;
    QString invoiceNo;
    QString sourceType;  // course | book
    QString sourceName;
    QString partnerName;
    Money saleValue{};
    Money commission{};
    Money partnerAmount{};
    Money myProfit{};
    bool reversal{};
    QString createdAt;
};

struct SourceReportRow {
    QString sourceId;
    QString name;
    QString partnerName;
    qint64 timesSold{};
    Money saleValue{};
    Money commission{};
    Money partnerAmount{};
    Money myProfit{};
};

struct PartnerReportRow {
    QString partnerId;
    QString name;
    Money earned{};   // partner share accrued in range (net of returns)
    Money paidOut{};  // payouts recorded in range
    Money balance{};  // current all-time balance still owed
};

struct ProfitSummary {
    Money totalSales{};
    Money totalCommission{};
    Money totalPartner{};
    Money myProfit{};
    Money totalPaidOut{};
    Money totalOwed{};
};

class PartnerService {
public:
    explicit PartnerService(std::shared_ptr<Database> database);

    // ---- Cashier-facing, UNGATED. Called inside the sale/return/void
    // transaction so commission is calculated and saved in the background on
    // every sale without the cashier ever authenticating. These read the
    // per-line snapshot columns already written on sale_items. ----
    void postSaleCommissions(const QString& saleId);
    void reverseSaleItem(const QString& saleId, const QString& saleItemId, Money returnedAmount, const QString& reason);
    void reverseSale(const QString& saleId, const QString& reason); // full cancellation of every line

    // Resolve the commission snapshot to stamp onto a sale line at checkout.
    // Ungated: needed on the cashier path. Returns zeros when nothing configured.
    CommissionConfig resolveCourseConfig(const QString& bundleId) const;
    CommissionConfig resolveBookConfig(const QString& productId) const;

    // ---- Owner-only, GATED. Every method below calls AuthSession::requireUnlocked()
    // first, so a locked/cashier session cannot read or change any of it. ----
    QString createPartner(const Partner& partner);
    void updatePartner(const Partner& partner);
    void archivePartner(const QString& partnerId);
    QList<Partner> listPartners(bool includeArchived = false) const;

    CommissionConfig courseConfig(const QString& bundleId) const;
    void setCourseConfig(const QString& bundleId, const CommissionConfig& config);
    CommissionConfig bookConfig(const QString& productId) const;
    void setBookConfig(const QString& productId, const CommissionConfig& config);
    QList<CommissionConfigRow> listCommissionConfigs(const QString& searchTerm) const;

    QList<PartnerLedgerRow> partnerLedger(const QString& partnerId, const QDate& from, const QDate& to) const;
    void recordPayout(const QString& partnerId, Money amount, const QString& note);

    QList<ProfitEntryRow> profitEntries(const QDate& from, const QDate& to) const;
    ProfitSummary summary(const QDate& from, const QDate& to) const;
    QList<SourceReportRow> courseReport(const QDate& from, const QDate& to) const;
    QList<SourceReportRow> standaloneBookReport(const QDate& from, const QDate& to) const;
    QList<PartnerReportRow> partnerReport(const QDate& from, const QDate& to) const;
    QList<SourceReportRow> partnerBreakdown(const QString& partnerId, const QDate& from, const QDate& to) const;

private:
    std::shared_ptr<Database> db_;
};
} // namespace pos
