#include "core/security_service.h"
#include "core/database.h"
#include "core/settings_service.h"
#include <QCryptographicHash>

namespace pos {
namespace {
bool validPin(const QString& pin){if(pin.size()<4||pin.size()>12)return false;for(const auto ch:pin)if(!ch.isDigit())return false;return true;}
QByteArray digest(const QString& salt,const QString& pin){return QCryptographicHash::hash((salt+":"+pin).toUtf8(),QCryptographicHash::Sha256).toHex();}
}
SecurityService::SecurityService(std::shared_ptr<Database> database):db_(std::move(database)){}
bool SecurityService::hasPin() const{return !SettingsService(db_).value("security.pin_hash").isEmpty();}
void SecurityService::setPin(const QString& pin){if(!validPin(pin))throw DatabaseError("PIN must contain 4 to 12 digits");const auto salt=uuid();SettingsService settings(db_);settings.setValue("security.pin_salt",salt);settings.setValue("security.pin_hash",QString::fromLatin1(digest(salt,pin)));}
bool SecurityService::verifyPin(const QString& pin) const {SettingsService settings(db_);const auto salt=settings.value("security.pin_salt");const auto expected=settings.value("security.pin_hash");return !salt.isEmpty()&&!expected.isEmpty()&&QString::fromLatin1(digest(salt,pin))==expected;}
void SecurityService::clearPin(){SettingsService settings(db_);settings.setValue("security.pin_salt",{});settings.setValue("security.pin_hash",{});}
}
