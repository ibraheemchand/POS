#include "core/settings_service.h"
#include "core/database.h"

namespace pos {
SettingsService::SettingsService(std::shared_ptr<Database> database) : db_(std::move(database)) {}

QString SettingsService::value(const QString& key, const QString& fallback) const {
    if (key.trimmed().isEmpty()) throw DatabaseError("setting key is required");
    auto query = db_->prepare("SELECT value FROM settings WHERE key=?");
    query.bind(1, key.trimmed());
    return query.stepRow() ? query.text(0) : fallback;
}

void SettingsService::setValue(const QString& key, const QString& value) {
    if (key.trimmed().isEmpty()) throw DatabaseError("setting key is required");
    Transaction transaction(db_->handle());
    auto query = db_->prepare("INSERT INTO settings(key,value,updated_at) VALUES(?,?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value,updated_at=excluded.updated_at");
    query.bind(1, key.trimmed());
    query.bind(2, value);
    query.bind(3, utcNow());
    query.execute();
    transaction.commit();
}
}
