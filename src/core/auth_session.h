#pragma once
#include <QString>
#include <memory>

namespace pos {
class Database;

// Process-wide gate for owner-only commission/partner/profit surfaces. Unlocking
// verifies the manager PIN (the same SecurityService PIN used for discount
// overrides); the session stays unlocked until lock() or an inactivity timeout
// elapses. Sensitive service methods call requireUnlocked() so the protection
// lives in the data layer, not only behind hidden UI buttons.
class AuthSession final {
public:
    static AuthSession& instance();

    // Returns false (and stays locked) if no PIN is configured or the PIN is wrong.
    bool unlock(const std::shared_ptr<Database>& database, const QString& pin);
    void lock();
    bool isUnlocked() const; // false once the inactivity timeout since the last touch elapses
    void touch();            // record activity to defer the inactivity lock
    void requireUnlocked() const; // throws DatabaseError when locked

    void setTimeoutSeconds(qint64 seconds);
    qint64 timeoutSeconds() const { return timeoutSeconds_; }

private:
    AuthSession() = default;
    bool unlocked_{false};
    qint64 lastActivityMs_{0};
    qint64 timeoutSeconds_{300}; // five minutes of inactivity re-locks the section
};
} // namespace pos
