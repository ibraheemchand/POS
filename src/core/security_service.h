#pragma once
#include <QString>
#include <memory>

namespace pos {
class Database;
class SecurityService final {
public:
    explicit SecurityService(std::shared_ptr<Database> database);
    bool hasPin() const;
    void setPin(const QString& pin);
    bool verifyPin(const QString& pin) const;
    void clearPin();
private:
    std::shared_ptr<Database> db_;
};
}
