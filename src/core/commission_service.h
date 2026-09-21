#pragma once
#include "core/types.h"
#include <memory>

namespace pos {
class Database;

// Rates are stored as basis points (1% = 100 bp) so the margin math stays in
// integers, matching the Money/Quantity convention used across the app.
struct CommissionSettings {
    qint64 commissionRateBp{3000};     // total markup pool taken from retail price
    qint64 partnerShareBp{1000};       // fixed, never reduced by a point-of-sale discount
    qint64 ownerMinShareBp{1000};      // floor guaranteed to the owner
};

struct CommissionBreakdown {
    Money retailAmount{};
    Money commissionAmount{};
    Money partnerAmount{};
    Money discountAmount{};
    Money ownerAmount{};
    Money flexibleCap{};
    bool overridden{};
};

struct CommissionTotals {
    Money revenue{};
    Money commissionPool{};
    Money partnerAccrued{};
    Money ownerProfit{};
    Money discountsGiven{};
    qint64 overrideCount{};
};

struct CommissionLedgerRow {
    QString invoiceNo;
    QString productName;
    Money retailAmount{};
    Money discountAmount{};
    Money partnerAmount{};
    Money ownerAmount{};
    bool overridden{};
    QString createdAt;
};

class CommissionService {
public:
    explicit CommissionService(std::shared_ptr<Database> database);

    CommissionSettings settings() const;
    void setSettings(const CommissionSettings& settings);

    // Largest discount (in paisa) a salesman may give on this line without a
    // manager override, i.e. the pool left after the partner and owner floors.
    Money flexibleCap(Money retailAmount, const CommissionSettings& snapshot) const;

    // Throws if discountAmount exceeds the flexible cap and overrideApproved is false.
    CommissionBreakdown computeBreakdown(Money retailAmount, Money discountAmount, bool overrideApproved, const CommissionSettings& snapshot) const;

    CommissionTotals totals(const QDate& from, const QDate& to) const;
    QList<CommissionLedgerRow> ledger(const QDate& from, const QDate& to, int limit) const;

private:
    std::shared_ptr<Database> db_;
};
} // namespace pos
