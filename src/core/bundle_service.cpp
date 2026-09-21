#include "core/bundle_service.h"
#include "core/database.h"

namespace pos {
namespace {
void validateBundle(const BundleDefinition& bundle) {
    if (bundle.name.trimmed().isEmpty()) throw DatabaseError("course name is required");
    if (bundle.items.isEmpty()) throw DatabaseError("a course needs at least one book");
    for (const auto& item : bundle.items) {
        if (item.productId.trimmed().isEmpty() || item.quantity <= 0) throw DatabaseError("invalid course item");
    }
}

void insertItems(Database& db, const QString& bundleId, const QList<BundleItemDefinition>& items) {
    int order = 0;
    for (const auto& item : items) {
        auto insert = db.prepare("INSERT INTO bundle_items(id,bundle_id,product_id,quantity,sort_order) VALUES(?,?,?,?,?)");
        insert.bind(1, uuid());
        insert.bind(2, bundleId);
        insert.bind(3, item.productId);
        insert.bind(4, item.quantity);
        insert.bind(5, static_cast<qint64>(order++));
        insert.execute();
    }
}
} // namespace

BundleService::BundleService(std::shared_ptr<Database> database) : db_(std::move(database)) {}

QString BundleService::createBundle(const BundleDefinition& bundle) {
    validateBundle(bundle);
    Transaction tx(db_->handle());
    for (const auto& item : bundle.items) {
        auto product = db_->prepare("SELECT id FROM products WHERE id=? AND is_deleted=0");
        product.bind(1, item.productId);
        if (!product.stepRow()) throw DatabaseError("a course book was not found or is archived");
    }
    const auto id = uuid();
    auto insert = db_->prepare("INSERT INTO bundles(id,name,grade_label,description,created_at) VALUES(?,?,?,?,?)");
    insert.bind(1, id);
    insert.bind(2, bundle.name.trimmed());
    insert.bind(3, bundle.gradeLabel.trimmed());
    insert.bind(4, bundle.description.trimmed());
    insert.bind(5, utcNow());
    insert.execute();
    insertItems(*db_, id, bundle.items);
    tx.commit();
    return id;
}

void BundleService::updateBundle(const QString& bundleId, const BundleDefinition& bundle) {
    validateBundle(bundle);
    Transaction tx(db_->handle());
    for (const auto& item : bundle.items) {
        auto product = db_->prepare("SELECT id FROM products WHERE id=? AND is_deleted=0");
        product.bind(1, item.productId);
        if (!product.stepRow()) throw DatabaseError("a course book was not found or is archived");
    }
    auto update = db_->prepare("UPDATE bundles SET name=?,grade_label=?,description=? WHERE id=? AND is_archived=0");
    update.bind(1, bundle.name.trimmed());
    update.bind(2, bundle.gradeLabel.trimmed());
    update.bind(3, bundle.description.trimmed());
    update.bind(4, bundleId);
    update.execute();
    if (sqlite3_changes(db_->handle()) != 1) throw DatabaseError("course not found");
    auto clear = db_->prepare("DELETE FROM bundle_items WHERE bundle_id=?");
    clear.bind(1, bundleId);
    clear.execute();
    insertItems(*db_, bundleId, bundle.items);
    tx.commit();
}

void BundleService::archiveBundle(const QString& bundleId) {
    Transaction tx(db_->handle());
    auto update = db_->prepare("UPDATE bundles SET is_archived=1 WHERE id=? AND is_archived=0");
    update.bind(1, bundleId);
    update.execute();
    if (sqlite3_changes(db_->handle()) != 1) throw DatabaseError("course not found");
    tx.commit();
}

QList<BundleSummary> BundleService::listBundles() const {
    QList<BundleSummary> result;
    auto query = db_->prepare(
        "SELECT b.id, b.name, b.grade_label, COUNT(i.id) FROM bundles b "
        "LEFT JOIN bundle_items i ON i.bundle_id=b.id "
        "WHERE b.is_archived=0 GROUP BY b.id ORDER BY b.name");
    while (query.stepRow()) {
        result.append({query.text(0), query.text(1), query.text(2), query.integer(3)});
    }
    return result;
}

BundleDefinition BundleService::findBundle(const QString& bundleId) const {
    auto bundle = db_->prepare("SELECT name, grade_label, description FROM bundles WHERE id=? AND is_archived=0");
    bundle.bind(1, bundleId);
    if (!bundle.stepRow()) throw DatabaseError("course not found");
    BundleDefinition result;
    result.name = bundle.text(0);
    result.gradeLabel = bundle.text(1);
    result.description = bundle.text(2);
    auto items = db_->prepare("SELECT product_id, quantity FROM bundle_items WHERE bundle_id=? ORDER BY sort_order");
    items.bind(1, bundleId);
    while (items.stepRow()) {
        result.items.append({items.text(0), items.integer(1)});
    }
    return result;
}

QList<BundleItemDisplay> BundleService::resolveItems(const QString& bundleId) const {
    auto bundle = db_->prepare("SELECT id FROM bundles WHERE id=? AND is_archived=0");
    bundle.bind(1, bundleId);
    if (!bundle.stepRow()) throw DatabaseError("course not found");

    QList<BundleItemDisplay> result;
    auto query = db_->prepare(
        "SELECT i.product_id, p.name, p.base_unit, i.quantity, p.retail_price_paisa, p.stock_quantity "
        "FROM bundle_items i JOIN products p ON p.id=i.product_id "
        "WHERE i.bundle_id=? AND p.is_deleted=0 ORDER BY i.sort_order");
    query.bind(1, bundleId);
    while (query.stepRow()) {
        result.append({query.text(0), query.text(1), query.text(2), query.integer(3), query.integer(4), query.integer(5)});
    }
    return result;
}
} // namespace pos
