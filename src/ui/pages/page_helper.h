#pragma once

#include "core/types.h"
#include "core/security_service.h"
#include <QString>
#include <QWidget>
#include <QInputDialog>
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
} // namespace pos
