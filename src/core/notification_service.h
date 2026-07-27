#pragma once
#include <QList>
#include <QString>
#include <memory>
namespace pos { class Database; struct Notification { QString id,type,title,body,createdAt; bool read{}; }; class NotificationService { public: explicit NotificationService(std::shared_ptr<Database> database); QString create(const QString& type,const QString& title,const QString& body); QList<Notification> unread(int limit=100) const; void markRead(const QString& id); private: std::shared_ptr<Database> db_; }; }
