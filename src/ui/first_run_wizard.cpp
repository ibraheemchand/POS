#include "ui/first_run_wizard.h"
#include "core/database.h"
#include "core/security_service.h"
#include "core/settings_service.h"
#include "core/seed_service.h"
#include "core/version.h"
#include <QWizardPage>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QMessageBox>
#include <QPrinterInfo>

FirstRunWizard::FirstRunWizard(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWizard(parent), database_(std::move(database)) {
    setWindowTitle(QString("%1 — first-time setup").arg(POS_APP_NAME));
    setWizardStyle(QWizard::ModernStyle);
    setMinimumSize(560, 420);
    setOption(QWizard::NoBackButtonOnStartPage, true);

    // Page 1 — business details.
    auto* business = new QWizardPage;
    business->setTitle("Welcome to " POS_APP_NAME);
    business->setSubTitle("Tell us about your business. You can change these later in Settings.");
    businessName_ = new QLineEdit; phone_ = new QLineEdit; address_ = new QLineEdit;
    businessName_->setObjectName("fw_business");
    auto* bform = new QFormLayout(business);
    bform->addRow("Business name*", businessName_);
    bform->addRow("Phone", phone_);
    bform->addRow("Address", address_);
    addPage(business);

    // Page 2 — owner PIN.
    auto* pinPage = new QWizardPage;
    pinPage->setTitle("Create your owner PIN");
    pinPage->setSubTitle("The PIN protects commission, partner and profit data. Minimum 6 digits.");
    pin_ = new QLineEdit; pin_->setEchoMode(QLineEdit::Password); pin_->setObjectName("fw_pin");
    pinConfirm_ = new QLineEdit; pinConfirm_->setEchoMode(QLineEdit::Password); pinConfirm_->setObjectName("fw_pinConfirm");
    auto* pform = new QFormLayout(pinPage);
    pform->addRow("Owner PIN (6–12 digits)*", pin_);
    pform->addRow("Confirm PIN*", pinConfirm_);
    auto* pinHint = new QLabel("A one-time recovery code will be shown after setup — write it down.");
    pinHint->setObjectName("muted"); pinHint->setWordWrap(true);
    pform->addRow(pinHint);
    addPage(pinPage);

    // Page 3 — printer (optional).
    auto* printerPage = new QWizardPage;
    printerPage->setTitle("Printer (optional)");
    printerPage->setSubTitle("Choose a receipt printer now, or skip and set it later in Settings.");
    printerCombo_ = new QComboBox;
    printerCombo_->addItem("— none / set up later —", QString());
    for (const auto& name : QPrinterInfo::availablePrinterNames()) printerCombo_->addItem(name, name);
    printerModeCombo_ = new QComboBox;
    printerModeCombo_->addItem("Thermal printer (raw ESC/POS)", "raw");
    printerModeCombo_->addItem("Normal printer (Windows driver)", "normal");
    auto* prform = new QFormLayout(printerPage);
    prform->addRow("Printer", printerCombo_);
    prform->addRow("Printing mode", printerModeCombo_);
    addPage(printerPage);

    // Page 4 — sample data (off by default).
    auto* samplePage = new QWizardPage;
    samplePage->setTitle("Start empty");
    samplePage->setSubTitle("Your new database is empty and ready for real data.");
    loadSample_ = new QCheckBox("Load sample data (demo products/customers) so I can try the app first");
    loadSample_->setObjectName("fw_loadSample");
    loadSample_->setChecked(false);
    auto* sl = new QVBoxLayout(samplePage);
    sl->addWidget(new QLabel("Leave this unchecked for a clean start. You can delete sample data later."));
    sl->addWidget(loadSample_);
    sl->addStretch();
    addPage(samplePage);
}

bool FirstRunWizard::validateCurrentPage() {
    // Page 0 = business details, page 1 = PIN. Block Next until they are valid.
    if (currentId() == 0 && businessName_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Business name required", "Please enter your business name.");
        return false;
    }
    if (currentId() == 1) {
        if (pin_->text().size() < 6) { QMessageBox::warning(this, "PIN too short", "The owner PIN must be at least 6 digits."); return false; }
        if (pin_->text() != pinConfirm_->text()) { QMessageBox::warning(this, "PINs do not match", "The two PIN entries do not match."); return false; }
    }
    return true;
}

bool FirstRunWizard::applySetup(QString* error) {
    try {
        pos::SecurityService security(database_);
        if (pin_->text() != pinConfirm_->text()) { if (error) *error = "The two PIN entries do not match."; return false; }
        recoveryCode_ = security.setupPin(pin_->text()); // enforces 6–12 digits, throws otherwise

        pos::SettingsService settings(database_);
        settings.setValue("business.name", businessName_->text().trimmed());
        settings.setValue("business.phone", phone_->text().trimmed());
        settings.setValue("business.address", address_->text().trimmed());
        const auto printer = printerCombo_->currentData().toString();
        if (!printer.isEmpty()) {
            settings.setValue("printer.thermal_path", printer);
            settings.setValue("printer.mode", printerModeCombo_->currentData().toString());
        }
        if (loadSample_->isChecked()) pos::SeedService(database_).seedDemoData();
        return true;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}

void FirstRunWizard::accept() {
    QString error;
    if (!applySetup(&error)) {
        QMessageBox::warning(this, "Setup incomplete", error);
        return; // stay on the wizard
    }
    QMessageBox::information(this, "Save your recovery code",
        "Setup complete.\n\nRecovery code (write it down — shown once; needed if you forget the PIN):\n\n"
        + recoveryCode_ + "\n\nThe app is offline, so there is no email reset.");
    QWizard::accept();
}
