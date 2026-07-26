#pragma once
#include <QString>
#include <QList>
#include <memory>
namespace pos { class Database; struct AuditEntry { QString action,entityType,entityId,detail,createdAt; }; class AuditService { public: explicit AuditService(std::shared_ptr<Database> db); QList<AuditEntry> recent(const QString& actionFilter={}, int limit=200) const; private: std::shared_ptr<Database> db_; }; }
