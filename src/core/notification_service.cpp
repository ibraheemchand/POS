#include "core/notification_service.h"
#include "core/database.h"

namespace pos {

NotificationService::NotificationService(std::shared_ptr<Database> database)
    : db_(std::move(database)) {}

NotificationService::NotificationService(Database& database)
    : db_(&database, [](Database*) {}) {}

QString NotificationService::create(const QString& type, const QString& title, const QString& body) {
    if (type.trimmed().isEmpty() || title.trimmed().isEmpty() || body.trimmed().isEmpty()) {
        throw DatabaseError("notification fields are required");
    }
    Transaction tx(db_->handle());
    const auto id = uuid();
    auto query = db_->prepare("INSERT INTO notifications(id,type,title,body,is_read,created_at) VALUES(?,?,?,?,0,?)");
    query.bind(1, id);
    query.bind(2, type.trimmed());
    query.bind(3, title.trimmed());
    query.bind(4, body.trimmed());
    query.bind(5, utcNow());
    query.execute();
    tx.commit();
    return id;
}

void NotificationService::createBestEffort(const QString& type, const QString& title, const QString& body) noexcept {
    try {
        create(type, title, body);
    } catch (...) {
        // Notifications must never make a committed business operation fail.
    }
}

QList<Notification> NotificationService::unread(int limit) const {
    if (limit < 1 || limit > 1000) throw DatabaseError("invalid notification limit");
    QList<Notification> result;
    auto query = db_->prepare("SELECT id,type,title,body,created_at,is_read FROM notifications WHERE is_read=0 ORDER BY created_at DESC LIMIT ?");
    query.bind(1, limit);
    while (query.stepRow()) result.append({query.text(0), query.text(1), query.text(2), query.text(3), query.text(4), query.integer(5) != 0});
    return result;
}

void NotificationService::markRead(const QString& id) {
    Transaction tx(db_->handle());
    auto query = db_->prepare("UPDATE notifications SET is_read=1 WHERE id=? AND is_read=0");
    query.bind(1, id);
    query.execute();
    if (sqlite3_changes(db_->handle()) != 1) throw DatabaseError("notification not found or already read");
    tx.commit();
}

} // namespace pos
