#include "core/security_service.h"
#include "core/database.h"
#include "core/settings_service.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>

namespace pos {
namespace {
constexpr int kMaxAttempts = 5;
constexpr int kLockSeconds = 300; // 5 minutes

bool validPin(const QString& pin) {
    if (pin.size() < 6 || pin.size() > 12) return false;
    for (const auto ch : pin) if (!ch.isDigit()) return false;
    return true;
}
QByteArray digest(const QString& salt, const QString& secret) {
    return QCryptographicHash::hash((salt + ":" + secret).toUtf8(), QCryptographicHash::Sha256).toHex();
}
bool matches(SettingsService& s, const QString& saltKey, const QString& hashKey, const QString& secret) {
    const auto salt = s.value(saltKey);
    const auto expected = s.value(hashKey);
    return !salt.isEmpty() && !expected.isEmpty() && QString::fromLatin1(digest(salt, secret)) == expected;
}
void store(SettingsService& s, const QString& saltKey, const QString& hashKey, const QString& secret) {
    const auto salt = uuid();
    s.setValue(saltKey, salt);
    s.setValue(hashKey, QString::fromLatin1(digest(salt, secret)));
}
// Human-friendly one-time code: three groups of four uppercase alphanumerics.
QString generateRecoveryCode() {
    static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // no ambiguous 0/O/1/I
    QString code;
    for (int i = 0; i < 12; ++i) {
        if (i && i % 4 == 0) code += '-';
        code += alphabet[QRandomGenerator::system()->bounded(int(sizeof(alphabet) - 1))];
    }
    return code;
}
QString normalizeCode(const QString& code) { return code.trimmed().toUpper().remove('-').remove(' '); }
} // namespace

SecurityService::SecurityService(std::shared_ptr<Database> database) : db_(std::move(database)) {}

bool SecurityService::hasPin() const { return !SettingsService(db_).value("security.pin_hash").isEmpty(); }

QString SecurityService::setupPin(const QString& pin) {
    if (hasPin()) throw DatabaseError("a PIN is already configured; use change or recovery");
    if (!validPin(pin)) throw DatabaseError("PIN must be 6 to 12 digits");
    SettingsService s(db_);
    store(s, "security.pin_salt", "security.pin_hash", pin);
    const auto recovery = generateRecoveryCode();
    store(s, "security.recovery_salt", "security.recovery_hash", normalizeCode(recovery));
    s.setValue("security.failed_attempts", "0");
    s.setValue("security.lock_until", {});
    return recovery;
}

void SecurityService::changePin(const QString& currentPin, const QString& newPin) {
    if (!verifyPin(currentPin)) throw DatabaseError("current PIN is incorrect");
    if (!validPin(newPin)) throw DatabaseError("PIN must be 6 to 12 digits");
    SettingsService s(db_);
    store(s, "security.pin_salt", "security.pin_hash", newPin);
}

QString SecurityService::resetWithRecovery(const QString& recoveryCode, const QString& newPin) {
    SettingsService s(db_);
    if (!matches(s, "security.recovery_salt", "security.recovery_hash", normalizeCode(recoveryCode)))
        throw DatabaseError("recovery code is incorrect");
    if (!validPin(newPin)) throw DatabaseError("PIN must be 6 to 12 digits");
    store(s, "security.pin_salt", "security.pin_hash", newPin);
    const auto recovery = generateRecoveryCode(); // the old code is single-use; issue a fresh one
    store(s, "security.recovery_salt", "security.recovery_hash", normalizeCode(recovery));
    s.setValue("security.failed_attempts", "0");
    s.setValue("security.lock_until", {});
    auto audit = db_->prepare("INSERT INTO audit_log(id,action,entity_type,entity_id,detail,created_at) VALUES(?,?,?,?,?,?)");
    audit.bind(1, uuid()); audit.bind(2, "pin_reset"); audit.bind(3, "security"); audit.bind(4, "owner_pin");
    audit.bind(5, "PIN reset using recovery code"); audit.bind(6, utcNow()); audit.execute();
    return recovery;
}

bool SecurityService::isLockedOut() const {
    const auto until = SettingsService(db_).value("security.lock_until");
    if (until.isEmpty()) return false;
    const auto lockUntil = QDateTime::fromString(until, Qt::ISODate);
    return lockUntil.isValid() && QDateTime::currentDateTimeUtc() < lockUntil;
}

int SecurityService::lockRemainingSeconds() const {
    const auto until = SettingsService(db_).value("security.lock_until");
    const auto lockUntil = QDateTime::fromString(until, Qt::ISODate);
    if (!lockUntil.isValid()) return 0;
    const auto secs = QDateTime::currentDateTimeUtc().secsTo(lockUntil);
    return secs > 0 ? static_cast<int>(secs) : 0;
}

SecurityService::UnlockResult SecurityService::attemptUnlock(const QString& pin) {
    if (isLockedOut()) return UnlockResult::LockedOut;
    SettingsService s(db_);
    if (verifyPin(pin)) { s.setValue("security.failed_attempts", "0"); return UnlockResult::Ok; }
    const auto attempts = s.value("security.failed_attempts", "0").toInt() + 1;
    if (attempts >= kMaxAttempts) {
        s.setValue("security.failed_attempts", "0");
        s.setValue("security.lock_until", QDateTime::currentDateTimeUtc().addSecs(kLockSeconds).toString(Qt::ISODate));
        auto audit = db_->prepare("INSERT INTO audit_log(id,action,entity_type,entity_id,detail,created_at) VALUES(?,?,?,?,?,?)");
        audit.bind(1, uuid()); audit.bind(2, "pin_lockout"); audit.bind(3, "security"); audit.bind(4, "owner_pin");
        audit.bind(5, QString("PIN entry locked for %1 minutes after %2 failed attempts").arg(kLockSeconds / 60).arg(kMaxAttempts));
        audit.bind(6, utcNow()); audit.execute();
        return UnlockResult::LockedOut;
    }
    s.setValue("security.failed_attempts", QString::number(attempts));
    return UnlockResult::Wrong;
}

bool SecurityService::verifyPin(const QString& pin) const {
    SettingsService s(db_);
    return matches(s, "security.pin_salt", "security.pin_hash", pin);
}

void SecurityService::clearPin() {
    SettingsService s(db_);
    for (const auto* key : {"security.pin_salt", "security.pin_hash", "security.recovery_salt",
                            "security.recovery_hash", "security.failed_attempts", "security.lock_until"})
        s.setValue(key, {});
}
} // namespace pos
