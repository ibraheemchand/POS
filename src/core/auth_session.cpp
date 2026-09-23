#include "core/auth_session.h"
#include "core/database.h"
#include "core/security_service.h"
#include <QDateTime>

namespace pos {

AuthSession& AuthSession::instance() {
    static AuthSession session;
    return session;
}

bool AuthSession::unlock(const std::shared_ptr<Database>& database, const QString& pin) {
    SecurityService security(database);
    if (!security.hasPin() || security.attemptUnlock(pin) != SecurityService::UnlockResult::Ok) {
        unlocked_ = false;
        return false;
    }
    unlocked_ = true;
    lastActivityMs_ = QDateTime::currentMSecsSinceEpoch();
    return true;
}

void AuthSession::lock() { unlocked_ = false; }

bool AuthSession::isUnlocked() const {
    if (!unlocked_) return false;
    if (timeoutSeconds_ <= 0) return false; // a zero/negative timeout means never trust the session
    const auto elapsedMs = QDateTime::currentMSecsSinceEpoch() - lastActivityMs_;
    return elapsedMs < timeoutSeconds_ * 1000;
}

void AuthSession::touch() {
    if (unlocked_) lastActivityMs_ = QDateTime::currentMSecsSinceEpoch();
}

void AuthSession::requireUnlocked() const {
    if (!isUnlocked()) throw DatabaseError("this section is locked; enter the owner PIN to continue");
}

void AuthSession::setTimeoutSeconds(qint64 seconds) { timeoutSeconds_ = seconds; }

} // namespace pos
