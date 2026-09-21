#pragma once
#include "core/types.h"
#include <memory>

namespace pos {
class Database;

struct BundleItemDefinition { QString productId; Quantity quantity{}; };
struct BundleDefinition { QString name; QString gradeLabel; QString description; QList<BundleItemDefinition> items; };
struct BundleSummary { QString id; QString name; QString gradeLabel; qint64 itemCount{}; };

// Live snapshot of a bundle line, resolved against the current catalog so a
// course always sells at today's price and reflects today's stock.
struct BundleItemDisplay {
    QString productId;
    QString productName;
    QString baseUnit;
    Quantity quantity{};
    Money retailPrice{};
    Quantity stock{};
};

class BundleService {
public:
    explicit BundleService(std::shared_ptr<Database> database);

    QString createBundle(const BundleDefinition& bundle);
    void updateBundle(const QString& bundleId, const BundleDefinition& bundle);
    void archiveBundle(const QString& bundleId);

    QList<BundleSummary> listBundles() const;
    BundleDefinition findBundle(const QString& bundleId) const;
    QList<BundleItemDisplay> resolveItems(const QString& bundleId) const;

private:
    std::shared_ptr<Database> db_;
};
} // namespace pos
