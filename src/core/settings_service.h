#pragma once
#include <QString>
#include <memory>

namespace pos {
class Database;

class SettingsService final {
public:
    explicit SettingsService(std::shared_ptr<Database> database);
    QString value(const QString& key, const QString& fallback = {}) const;
    void setValue(const QString& key, const QString& value);

private:
    std::shared_ptr<Database> db_;
};
}
