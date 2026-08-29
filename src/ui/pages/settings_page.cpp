#include "ui/pages/settings_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/settings_service.h"
#include "core/security_service.h"
#include "core/notification_service.h"
#include "core/thermal_print_service.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>

SettingsPage::SettingsPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(database) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    auto* title = new QLabel("Settings", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    // Business Identity Frame
    auto* identityCard = new QFrame(this);
    identityCard->setObjectName("panel");
    auto* identityLayout = new QVBoxLayout(identityCard);
    identityLayout->setContentsMargins(20, 18, 20, 18);
    identityLayout->setSpacing(14);

    auto* identityTitle = new QLabel("Business identity & defaults", identityCard);
    identityTitle->setObjectName("sectionTitle");
    identityLayout->addWidget(identityTitle);

    businessNameInput_ = new QLineEdit(identityCard);
    phoneInput_ = new QLineEdit(identityCard);
    currencyInput_ = new QLineEdit(identityCard);
    footerInput_ = new QLineEdit(identityCard);

    backupHoursSpin_ = new QSpinBox(identityCard);
    backupHoursSpin_->setRange(0, 168);
    backupHoursSpin_->setSuffix(" hours (0 disables)");

    thermalPathInput_ = new QLineEdit(identityCard);
    thermalPathInput_->setPlaceholderText("Raw printer path or shared printer device");

    auto* form = new QGridLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(12);
    form->setColumnStretch(1, 1);

    auto* busLabel = new QLabel("Business name", identityCard);
    busLabel->setBuddy(businessNameInput_);

    auto* phoneLabel = new QLabel("Phone", identityCard);
    phoneLabel->setBuddy(phoneInput_);

    auto* curLabel = new QLabel("Currency", identityCard);
    curLabel->setBuddy(currencyInput_);

    auto* footerLabel = new QLabel("Receipt footer", identityCard);
    footerLabel->setBuddy(footerInput_);

    auto* backupLabel = new QLabel("Automatic backup interval", identityCard);
    backupLabel->setBuddy(backupHoursSpin_);

    auto* thermLabel = new QLabel("Thermal printer path", identityCard);
    thermLabel->setBuddy(thermalPathInput_);

    form->addWidget(busLabel, 0, 0);
    form->addWidget(businessNameInput_, 0, 1);
    form->addWidget(phoneLabel, 1, 0);
    form->addWidget(phoneInput_, 1, 1);
    form->addWidget(curLabel, 2, 0);
    form->addWidget(currencyInput_, 2, 1);
    form->addWidget(footerLabel, 3, 0);
    form->addWidget(footerInput_, 3, 1);
    form->addWidget(backupLabel, 4, 0);
    form->addWidget(backupHoursSpin_, 4, 1);
    form->addWidget(thermLabel, 5, 0);
    form->addWidget(thermalPathInput_, 5, 1);
    identityLayout->addLayout(form);

    saveSettingsBtn_ = new QPushButton("Save settings", identityCard);
    saveSettingsBtn_->setObjectName("primary");
    auto* saveRow = new QHBoxLayout;
    saveRow->addWidget(saveSettingsBtn_);
    saveRow->addStretch();
    identityLayout->addLayout(saveRow);
    layout->addWidget(identityCard);

    // Security Card Frame
    auto* securityCard = new QFrame(this);
    securityCard->setObjectName("panel");
    auto* securityLayout = new QVBoxLayout(securityCard);
    securityLayout->setContentsMargins(20, 18, 20, 18);
    securityLayout->setSpacing(12);

    auto* securityTitle = new QLabel("Security, printing & notifications", securityCard);
    securityTitle->setObjectName("sectionTitle");
    securityLayout->addWidget(securityTitle);

    setPinBtn_ = new QPushButton("Set or change security PIN", securityCard);
    clearPinBtn_ = new QPushButton("Clear security PIN", securityCard);
    clearPinBtn_->setObjectName("danger");
    
    viewNotificationsBtn_ = new QPushButton("View notifications", securityCard);
    testReceiptBtn_ = new QPushButton("Print test receipt", securityCard);
    testLabelBtn_ = new QPushButton("Print test barcode label", securityCard);
    
    pinStatusLabel_ = new QLabel(securityCard);
    pinStatusLabel_->setObjectName("muted");

    securityLayout->addWidget(setPinBtn_);
    securityLayout->addWidget(clearPinBtn_);
    securityLayout->addWidget(viewNotificationsBtn_);
    securityLayout->addWidget(testReceiptBtn_);
    securityLayout->addWidget(testLabelBtn_);
    securityLayout->addWidget(pinStatusLabel_);
    layout->addWidget(securityCard);
    layout->addStretch();

    // Initial setups
    QTimer::singleShot(0, this, [this] { load(); });

    // Connections
    connect(saveSettingsBtn_, &QPushButton::clicked, this, [this] {
        try {
            pos::SettingsService service(database_);
            service.setValue("business.name", businessNameInput_->text().trimmed());
            service.setValue("business.phone", phoneInput_->text().trimmed());
            service.setValue("business.currency", currencyInput_->text().trimmed().isEmpty() ? "PKR" : currencyInput_->text().trimmed());
            service.setValue("receipt.footer", footerInput_->text());
            service.setValue("backup.interval_hours", QString::number(backupHoursSpin_->value()));
            service.setValue("printer.thermal_path", thermalPathInput_->text().trimmed());
            QMessageBox::information(this, "Settings saved", "Settings were saved to the local database. Restart the app to apply a changed automatic backup interval.");
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not save settings", error.what());
        }
    });

    connect(testReceiptBtn_, &QPushButton::clicked, this, [this] {
        try {
            const auto path = pos::SettingsService(database_).value("printer.thermal_path");
            pos::ThermalPrintService::writeRaw(path, pos::ThermalPrintService::receiptBytes("Nexora POS", "TEST-1", {{"Printer test", 1, 100}}, 100));
            QMessageBox::information(this, "Receipt sent", "The ESC/POS test receipt was sent to the configured device.");
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not print receipt", error.what());
        }
    });

    connect(testLabelBtn_, &QPushButton::clicked, this, [this] {
        try {
            const auto path = pos::SettingsService(database_).value("printer.thermal_path");
            pos::ThermalPrintService::writeRaw(path, pos::ThermalPrintService::barcodeLabelBytes("Nexora test label", "123456789012"));
            QMessageBox::information(this, "Label sent", "The Code128 test label was sent to the configured device.");
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not print label", error.what());
        }
    });

    connect(setPinBtn_, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto first = QInputDialog::getText(this, "Set security PIN", "PIN (4-12 digits):", QLineEdit::Password, {}, &ok);
        if (!ok) return;
        const auto second = QInputDialog::getText(this, "Set security PIN", "Confirm PIN:", QLineEdit::Password, {}, &ok);
        if (!ok || first != second) {
            QMessageBox::warning(this, "PIN not changed", "The PIN confirmation did not match.");
            return;
        }
        try {
            pos::SecurityService(database_).setPin(first);
            pinStatusLabel_->setText("Sensitive-action PIN: configured");
            QMessageBox::information(this, "PIN saved", "The security PIN is stored as a salted hash in the local database.");
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not set PIN", error.what());
        }
    });

    connect(clearPinBtn_, &QPushButton::clicked, this, [this] {
        if (!pos::authorizeSensitiveAction(this, database_, "clear the security PIN")) return;
        if (QMessageBox::question(this, "Clear security PIN", "Remove the configured security PIN?") == QMessageBox::Yes) {
            try {
                pos::SecurityService(database_).clearPin();
                pinStatusLabel_->setText("Sensitive-action PIN: not configured");
            } catch (const std::exception& error) {
                QMessageBox::critical(this, "Could not clear PIN", error.what());
            }
        }
    });

    connect(viewNotificationsBtn_, &QPushButton::clicked, this, [this] {
        try {
            const auto items = pos::NotificationService(database_).unread();
            if (items.isEmpty()) {
                QMessageBox::information(this, "Notifications", "You have no unread notifications.");
                return;
            }
            QString text;
            for (const auto& item : items) {
                text += QString("[%1] %2\n%3\n\n").arg(item.createdAt, item.title, item.body);
                pos::NotificationService(database_).markRead(item.id);
            }
            QMessageBox::information(this, "Notifications", text.trimmed());
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not load notifications", error.what());
        }
    });

    // Set tab order
    setTabOrder(businessNameInput_, phoneInput_);
    setTabOrder(phoneInput_, currencyInput_);
    setTabOrder(currencyInput_, footerInput_);
    setTabOrder(footerInput_, backupHoursSpin_);
    setTabOrder(backupHoursSpin_, thermalPathInput_);
    setTabOrder(thermalPathInput_, saveSettingsBtn_);
    setTabOrder(saveSettingsBtn_, setPinBtn_);
    setTabOrder(setPinBtn_, clearPinBtn_);
    setTabOrder(clearPinBtn_, viewNotificationsBtn_);
    setTabOrder(viewNotificationsBtn_, testReceiptBtn_);
    setTabOrder(testReceiptBtn_, testLabelBtn_);
}

void SettingsPage::load() {
    try {
        pos::SettingsService service(database_);
        businessNameInput_->setText(service.value("business.name"));
        phoneInput_->setText(service.value("business.phone"));
        currencyInput_->setText(service.value("business.currency", "PKR"));
        footerInput_->setText(service.value("receipt.footer"));
        backupHoursSpin_->setValue(service.value("backup.interval_hours", "0").toInt());
        thermalPathInput_->setText(service.value("printer.thermal_path"));
        
        const bool pinConfigured = pos::SecurityService(database_).hasPin();
        pinStatusLabel_->setText(pinConfigured ? "Sensitive-action PIN: configured" : "Sensitive-action PIN: not configured");
    } catch (...) {}
}
