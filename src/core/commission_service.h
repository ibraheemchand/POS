#pragma once
#include "core/types.h"
#include <memory>

namespace pos {
class Database;

// The only global discount-limit knob left. Per-item total%/partner% now live on
// products/bundles (see PartnerService); the flexible discount margin for a line
// is derived from those minus this owner floor. Stored as basis points.
struct CommissionSettings {
    qint64 ownerMinShareBp{1200}; // owner's guaranteed floor (default 12%)
};

struct CommissionBreakdown {
    Money retailAmount{};       // gross line amount, before discount
    Money commissionAmount{};   // gross * totalBp
    Money discountAmount{};
    Money ownerAmount{};        // commission - discount
    Money flexibleCap{};
    bool overridden{};
};

struct CommissionTotals {
    Money revenue{};
    Money commissionPool{};
    Money ownerProfit{};
    Money discountsGiven{};
    qint64 overrideCount{};
};

struct CommissionLedgerRow {
    QString invoiceNo;
    QString productName;
    Money retailAmount{};
    Money discountAmount{};
    Money ownerAmount{};
    bool overridden{};
    QString createdAt;
};

class CommissionService {
public:
    explicit CommissionService(std::shared_ptr<Database> database);

    CommissionSettings settings() const;
    void setSettings(const CommissionSettings& settings);

    // Largest discount (paisa) allowed on this line without a manager override:
    // (item total% - item partner% - owner min%) of the gross line amount, floored
    // at 0. An item with no commission configured (total%=0) yields a cap of 0.
    Money flexibleCap(Money grossLineAmount, qint64 totalBp, qint64 partnerBp) const;

    // Throws if discount exceeds the cap and overrideApproved is false.
    CommissionBreakdown computeBreakdown(Money grossLineAmount, qint64 totalBp, qint64 partnerBp, Money discountAmount, bool overrideApproved) const;

    CommissionTotals totals(const QDate& from, const QDate& to) const;
    QList<CommissionLedgerRow> ledger(const QDate& from, const QDate& to, int limit) const;

private:
    std::shared_ptr<Database> db_;
};
} // namespace pos
