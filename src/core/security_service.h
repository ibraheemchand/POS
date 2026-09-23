#pragma once
#include <QString>
#include <memory>

namespace pos {
class Database;

// Owner PIN with offline recovery and brute-force lockout. The PIN is created
// once at first run (setupPin) — never silently by whoever opens an owner page.
class SecurityService final {
public:
    enum class UnlockResult { Ok, Wrong, LockedOut };

    explicit SecurityService(std::shared_ptr<Database> database);

    bool hasPin() const;

    // First-run setup: sets the PIN and generates a one-time recovery code, which
    // is returned in plaintext exactly once (store it, show it, never again).
    // Throws if a PIN already exists or the PIN is invalid (must be 6-12 digits).
    QString setupPin(const QString& pin);

    // Requires the current PIN; throws if it is wrong or the new PIN is invalid.
    void changePin(const QString& currentPin, const QString& newPin);

    // Offline "forgot PIN": verifies the recovery code, sets a new PIN, and issues
    // a fresh recovery code (returned once). Throws if the recovery code is wrong.
    QString resetWithRecovery(const QString& recoveryCode, const QString& newPin);

    // Lockout-aware unlock used by every PIN prompt. Counts failures; after 5 wrong
    // attempts it locks entry for 5 minutes and records the lockout in the audit log.
    UnlockResult attemptUnlock(const QString& pin);

    bool isLockedOut() const;
    int lockRemainingSeconds() const;

    bool verifyPin(const QString& pin) const; // pure check, no attempt counting
    void clearPin();                          // clears PIN, recovery, attempts (setup/reset/tests)

private:
    std::shared_ptr<Database> db_;
};
} // namespace pos
