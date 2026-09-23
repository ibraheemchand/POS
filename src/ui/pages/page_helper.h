#pragma once

#include "core/types.h"
#include "core/security_service.h"
#include "core/auth_session.h"
#include <QString>
#include <QWidget>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <memory>

namespace pos {
inline QString formatPaisa(Money value) {
    const auto sign = value < 0 ? "-" : "";
    const auto absolute = value < 0 ? -value : value;
    return QString("%1%2.%3").arg(sign).arg(absolute / 100).arg(absolute % 100, 2, 10, QChar('0'));
}

inline bool authorizeSensitiveAction(QWidget* parent, std::shared_ptr<Database> database, const QString& action) {
    SecurityService security(database);
    if (!security.hasPin()) {
        QMessageBox::warning(parent, "PIN required", QString("Configure a security PIN before %1.").arg(action));
        return false;
    }
    bool ok = false;
    const auto pin = QInputDialog::getText(parent, "Security PIN", QString("Enter PIN to %1:").arg(action), QLineEdit::Password, {}, &ok);
    if (!ok || !security.verifyPin(pin)) {
        QMessageBox::warning(parent, "Action denied", "The security PIN was incorrect.");
        return false;
    }
    return true;
}

// Unlocks the owner-only AuthSession that gates every commission/partner/profit
// surface. Distinct from authorizeSensitiveAction (a one-shot approval that must
// NOT unlock the whole session, e.g. a manager approving a single discount).
// Offers to create a PIN the first time one is needed.
inline bool unlockOwnerSession(QWidget* parent, std::shared_ptr<Database> database) {
    auto& session = AuthSession::instance();
    if (session.isUnlocked()) { session.touch(); return true; }
    SecurityService security(database);
    if (!security.hasPin()) {
        if (QMessageBox::question(parent, "Set owner PIN",
                "No owner PIN is set yet. Set one now to protect commission, partner and profit data?") != QMessageBox::Yes)
            return false;
        bool ok = false;
        const auto newPin = QInputDialog::getText(parent, "Set owner PIN", "Choose a PIN (4-12 digits):", QLineEdit::Password, {}, &ok);
        if (!ok) return false;
        try { security.setPin(newPin); }
        catch (const std::exception& error) { QMessageBox::warning(parent, "PIN not set", error.what()); return false; }
    }
    bool ok = false;
    const auto pin = QInputDialog::getText(parent, "Owner PIN", "Enter the owner PIN to continue:", QLineEdit::Password, {}, &ok);
    if (!ok) return false;
    if (!session.unlock(database, pin)) {
        QMessageBox::warning(parent, "Locked", "The owner PIN was incorrect.");
        return false;
    }
    return true;
}
} // namespace pos
