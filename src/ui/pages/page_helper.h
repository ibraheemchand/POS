#pragma once

#include "core/types.h"
#include "core/security_service.h"
#include "core/auth_session.h"
#include <QString>
#include <QWidget>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidgetItem>
#include <memory>

namespace pos {
// formatMoney(), roundMoney() and parseMoney() live in core/types.h.

// A right-aligned "PKR ..." table cell for money columns (digits line up).
inline QTableWidgetItem* moneyItem(Money value) {
    auto* item = new QTableWidgetItem("PKR " + formatMoney(value));
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

// Prompt for a rupee amount as text, parse to exact hundredths, and reject more
// than 2 decimals with a clear message. `current` is the existing amount (hundredths)
// used to prefill. Sets ok=false if cancelled.
inline Money askMoney(QWidget* parent, const QString& title, const QString& label, Money current, bool* ok) {
    QString initial = current != 0 ? (current % 100 == 0 ? QString::number(current / 100)
                                                         : QString::number(current / 100.0, 'f', 2)) : QString();
    for (;;) {
        bool got = false;
        const auto text = QInputDialog::getText(parent, title, label + " (PKR, up to 2 decimals):",
                                                QLineEdit::Normal, initial, &got);
        if (!got) { if (ok) *ok = false; return 0; }
        bool parsed = false;
        const Money value = parseMoney(text, &parsed);
        if (!parsed || value < 0) {
            QMessageBox::warning(parent, title, "Enter a valid amount in rupees — e.g. 1500 or 1500.50 (at most 2 decimals).");
            initial = text;
            continue;
        }
        if (ok) *ok = true;
        return value;
    }
}

// One-shot manager approval (e.g. approving a single over-cap discount). Does NOT
// unlock the owner session. Lockout-aware: 5 wrong PINs anywhere locks entry.
inline bool authorizeSensitiveAction(QWidget* parent, std::shared_ptr<Database> database, const QString& action) {
    SecurityService security(database);
    if (!security.hasPin()) {
        QMessageBox::warning(parent, "PIN required", "No owner PIN is configured. Restart the app to run first-time setup.");
        return false;
    }
    if (security.isLockedOut()) {
        QMessageBox::warning(parent, "Locked", QString("Too many wrong attempts. Try again in about %1 minute(s).").arg((security.lockRemainingSeconds() + 59) / 60));
        return false;
    }
    bool ok = false;
    const auto pin = QInputDialog::getText(parent, "Manager PIN", QString("Enter the owner/manager PIN to %1:").arg(action), QLineEdit::Password, {}, &ok);
    if (!ok) return false;
    switch (security.attemptUnlock(pin)) {
        case SecurityService::UnlockResult::Ok: return true;
        case SecurityService::UnlockResult::LockedOut:
            QMessageBox::warning(parent, "Locked", "Too many wrong attempts — PIN entry is locked for 5 minutes.");
            return false;
        default:
            QMessageBox::warning(parent, "Action denied", "The PIN was incorrect.");
            return false;
    }
}

// Offline "forgot PIN": one dialog with recovery code + new PIN + confirm. On
// success it resets the PIN, shows the fresh recovery code once, unlocks the
// session and returns true.
inline bool recoverOwnerPin(QWidget* parent, std::shared_ptr<Database> database) {
    QDialog dialog(parent);
    dialog.setWindowTitle("Reset PIN with recovery code");
    dialog.setMinimumWidth(400);
    auto* layout = new QVBoxLayout(&dialog);
    auto* intro = new QLabel("Enter the recovery code you saved at setup, then choose a new PIN.", &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* form = new QFormLayout;
    auto* codeEdit = new QLineEdit(&dialog);
    auto* pinEdit = new QLineEdit(&dialog); pinEdit->setEchoMode(QLineEdit::Password);
    auto* confirmEdit = new QLineEdit(&dialog); confirmEdit->setEchoMode(QLineEdit::Password);
    form->addRow("Recovery code", codeEdit);
    form->addRow("New PIN (6-12 digits)", pinEdit);
    form->addRow("Confirm new PIN", confirmEdit);
    layout->addLayout(form);

    auto* error = new QLabel(&dialog); error->setObjectName("danger"); error->setWordWrap(true);
    layout->addWidget(error);

    auto* buttons = new QHBoxLayout;
    auto* cancel = new QPushButton("Cancel", &dialog);
    auto* reset = new QPushButton("Reset PIN", &dialog); reset->setObjectName("primary"); reset->setDefault(true);
    buttons->addStretch(); buttons->addWidget(cancel); buttons->addWidget(reset);
    layout->addLayout(buttons);

    bool unlocked = false;
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(reset, &QPushButton::clicked, &dialog, [&] {
        if (pinEdit->text() != confirmEdit->text()) { error->setText("The two PIN entries do not match."); return; }
        try {
            const auto newRecovery = SecurityService(database).resetWithRecovery(codeEdit->text(), pinEdit->text());
            QMessageBox::information(&dialog, "PIN reset",
                "Your PIN has been reset.\n\nNew recovery code (write it down — shown once):\n\n" + newRecovery
                + "\n\nThe app is offline, so there is no email reset.");
            unlocked = AuthSession::instance().unlock(database, pinEdit->text());
            dialog.accept();
        } catch (const std::exception& e) { error->setText(e.what()); }
    });
    dialog.exec();
    return unlocked;
}

// Unlocks the owner-only AuthSession behind commission/partner/profit surfaces.
// Single dialog: PIN field, Unlock, and a "Forgot PIN?" link. The PIN must already
// exist (created at first run) — this never creates one.
inline bool unlockOwnerSession(QWidget* parent, std::shared_ptr<Database> database) {
    auto& session = AuthSession::instance();
    if (session.isUnlocked()) { session.touch(); return true; }
    SecurityService security(database);
    if (!security.hasPin()) {
        QMessageBox::warning(parent, "Locked", "No owner PIN is configured. Restart the app to run first-time setup.");
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle("Owner PIN");
    dialog.setMinimumWidth(340);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Enter the owner PIN to continue.", &dialog));
    auto* pinEdit = new QLineEdit(&dialog); pinEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(pinEdit);
    auto* error = new QLabel(&dialog); error->setObjectName("danger"); error->setWordWrap(true);
    layout->addWidget(error);

    auto* buttons = new QHBoxLayout;
    auto* forgot = new QPushButton("Forgot PIN?", &dialog); forgot->setFlat(true); forgot->setCursor(Qt::PointingHandCursor);
    auto* cancel = new QPushButton("Cancel", &dialog);
    auto* unlock = new QPushButton("Unlock", &dialog); unlock->setObjectName("primary"); unlock->setDefault(true);
    buttons->addWidget(forgot); buttons->addStretch(); buttons->addWidget(cancel); buttons->addWidget(unlock);
    layout->addLayout(buttons);

    bool unlocked = false;
    const auto applyLockState = [&] {
        if (security.isLockedOut()) {
            error->setText(QString("Locked. Try again in about %1 minute(s).").arg((security.lockRemainingSeconds() + 59) / 60));
            pinEdit->setEnabled(false); unlock->setEnabled(false);
        }
    };
    applyLockState();

    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(unlock, &QPushButton::clicked, &dialog, [&] {
        if (session.unlock(database, pinEdit->text())) { unlocked = true; dialog.accept(); return; }
        if (security.isLockedOut()) { error->setText("Too many wrong attempts — PIN entry is locked for 5 minutes."); pinEdit->setEnabled(false); unlock->setEnabled(false); return; }
        error->setText("Incorrect PIN."); pinEdit->clear(); pinEdit->setFocus();
    });
    QObject::connect(pinEdit, &QLineEdit::returnPressed, unlock, &QPushButton::click);
    QObject::connect(forgot, &QPushButton::clicked, &dialog, [&] {
        if (recoverOwnerPin(&dialog, database)) { unlocked = true; dialog.accept(); }
    });
    dialog.exec();
    return unlocked;
}
} // namespace pos
