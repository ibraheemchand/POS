#include "ui/pages/settings_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/settings_service.h"
#include "core/security_service.h"
#include "core/notification_service.h"
#include "core/thermal_print_service.h"
#include "core/receipt_service.h"
#include "core/app_paths.h"
#include "core/version.h"
#include "ui/receipt_output.h"
#include <QVBoxLayout>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QMessageBox>
#include <QComboBox>
#include <QPrinterInfo>
#include <QDateTime>
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

    printerModeCombo_ = new QComboBox(identityCard);
    printerModeCombo_->addItem("Thermal printer (raw ESC/POS)", "raw");
    printerModeCombo_->addItem("Normal printer (Windows driver)", "normal");

    paperSizeCombo_ = new QComboBox(identityCard);
    paperSizeCombo_->addItem("80 mm roll (table layout)", "80");
    paperSizeCombo_->addItem("58 mm roll (compact)", "58");

    auto* form = new QGridLayout;
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(12);
    form->setColumnStretch(1, 1);

    auto* busLabel = new QLabel("Business name", identityCard); busLabel->setBuddy(businessNameInput_);
    auto* phoneLabel = new QLabel("Phone", identityCard); phoneLabel->setBuddy(phoneInput_);
    auto* curLabel = new QLabel("Currency", identityCard); curLabel->setBuddy(currencyInput_);
    auto* footerLabel = new QLabel("Receipt footer", identityCard); footerLabel->setBuddy(footerInput_);
    auto* backupLabel = new QLabel("Automatic backup interval", identityCard); backupLabel->setBuddy(backupHoursSpin_);
    auto* thermLabel = new QLabel("Printer path / name", identityCard); thermLabel->setBuddy(thermalPathInput_);
    auto* modeLabel = new QLabel("Printing mode", identityCard); modeLabel->setBuddy(printerModeCombo_);

    form->addWidget(busLabel, 0, 0);        form->addWidget(businessNameInput_, 0, 1);
    form->addWidget(phoneLabel, 1, 0);      form->addWidget(phoneInput_, 1, 1);
    form->addWidget(curLabel, 2, 0);        form->addWidget(currencyInput_, 2, 1);
    form->addWidget(footerLabel, 3, 0);     form->addWidget(footerInput_, 3, 1);
    form->addWidget(backupLabel, 4, 0);     form->addWidget(backupHoursSpin_, 4, 1);
    form->addWidget(thermLabel, 5, 0);      form->addWidget(thermalPathInput_, 5, 1);
    form->addWidget(modeLabel, 6, 0);       form->addWidget(printerModeCombo_, 6, 1);
    auto* paperLabel2 = new QLabel("Receipt paper", identityCard); paperLabel2->setBuddy(paperSizeCombo_);
    form->addWidget(paperLabel2, 11, 0);    form->addWidget(paperSizeCombo_, 11, 1);

    // Printer picker: pick an installed printer and its name fills the field above.
    // Raw ESC/POS is sent through the Windows spooler, which fixes the old
    // "Access is denied" (that came from opening the printer as a file).
    auto* printerCombo = new QComboBox(identityCard);
    printerCombo->addItem("— pick an installed printer —", QString());
    for (const auto& name : QPrinterInfo::availablePrinterNames()) printerCombo->addItem(name, name);
    auto* printerLabel = new QLabel("Detected printers", identityCard); printerLabel->setBuddy(printerCombo);
    auto* printerHint = new QLabel("Choose your printer here (fills the path above). Thermal receipt printers use raw mode; Microsoft Print to PDF, XPS and office (A4/A5) printers use normal mode. For a network printer, share it and enter \\\\PC-NAME\\ShareName in the path field.", identityCard);
    printerHint->setObjectName("muted"); printerHint->setWordWrap(true);
    form->addWidget(printerLabel, 7, 0);    form->addWidget(printerCombo, 7, 1);
    form->addWidget(printerHint, 8, 0, 1, 2);

    // Extra backup location (USB / another drive) so a copy lives outside the data folder.
    extraBackupInput_ = new QLineEdit(identityCard);
    extraBackupInput_->setPlaceholderText("e.g. E:\\InventoBackups  (USB or another drive)");
    auto* extraBrowse = new QPushButton("Browse…", identityCard);
    auto* extraRow = new QHBoxLayout;
    extraRow->addWidget(extraBackupInput_, 1);
    extraRow->addWidget(extraBrowse);
    auto* extraLabel = new QLabel("Extra backup folder", identityCard); extraLabel->setBuddy(extraBackupInput_);
    extraBackupStatus_ = new QLabel(identityCard); extraBackupStatus_->setObjectName("muted"); extraBackupStatus_->setWordWrap(true);
    form->addWidget(extraLabel, 9, 0);      form->addLayout(extraRow, 9, 1);
    form->addWidget(extraBackupStatus_, 10, 0, 1, 2);
    connect(extraBrowse, &QPushButton::clicked, this, [this] {
        const auto dir = QFileDialog::getExistingDirectory(this, "Choose an extra backup folder");
        if (!dir.isEmpty()) { extraBackupInput_->setText(QDir::toNativeSeparators(dir)); }
    });
    const auto refreshExtraStatus = [this] {
        const auto dir = extraBackupInput_->text().trimmed();
        if (dir.isEmpty()) { extraBackupStatus_->setText("No extra backup location set. A single copy is kept in the data folder only."); return; }
        extraBackupStatus_->setText(QDir(dir).exists()
            ? QString("✓ Reachable. Scheduled backups will also be copied here.")
            : QString("⚠ Not reachable right now (drive unplugged?). Off-site copies will be skipped until it is available."));
    };
    connect(extraBackupInput_, &QLineEdit::textChanged, this, [refreshExtraStatus](const QString&) { refreshExtraStatus(); });
    connect(printerCombo, &QComboBox::currentIndexChanged, this, [this, printerCombo](int) {
        const auto name = printerCombo->currentData().toString();
        if (name.isEmpty()) return;
        thermalPathInput_->setText(name);
        // Auto-select normal mode for non-thermal targets (PDF/XPS/office printers).
        if (pos::ui::isLikelyNonThermal(name)) {
            printerModeCombo_->setCurrentIndex(printerModeCombo_->findData("normal"));
        }
    });
    identityLayout->addLayout(form);

    saveSettingsBtn_ = new QPushButton("Save settings", identityCard);
    saveSettingsBtn_->setObjectName("primary");
    auto* saveRow = new QHBoxLayout;
    saveRow->addWidget(saveSettingsBtn_);
    saveRow->addStretch();
    identityLayout->addLayout(saveRow);
    layout->addWidget(identityCard);

    // Security / printing / notifications. PIN creation and changes are NOT here:
    // the PIN is created once at first run and changed only from the password-
    // protected Commission Settings page. This card only shows status + safe tools.
    auto* securityCard = new QFrame(this);
    securityCard->setObjectName("panel");
    auto* securityLayout = new QVBoxLayout(securityCard);
    securityLayout->setContentsMargins(20, 18, 20, 18);
    securityLayout->setSpacing(12);

    auto* securityTitle = new QLabel("Security, printing & notifications", securityCard);
    securityTitle->setObjectName("sectionTitle");
    securityLayout->addWidget(securityTitle);

    pinStatusLabel_ = new QLabel(securityCard);
    pinStatusLabel_->setObjectName("muted");
    pinStatusLabel_->setWordWrap(true);

    viewNotificationsBtn_ = new QPushButton("View notifications", securityCard);
    testReceiptBtn_ = new QPushButton("Print test receipt", securityCard);
    testLabelBtn_ = new QPushButton("Print test barcode label", securityCard);

    securityLayout->addWidget(pinStatusLabel_);
    securityLayout->addWidget(viewNotificationsBtn_);
    securityLayout->addWidget(testReceiptBtn_);
    securityLayout->addWidget(testLabelBtn_);
    layout->addWidget(securityCard);

    // About: version + where data lives, with quick access to the folders.
    auto* aboutCard = new QFrame(this);
    aboutCard->setObjectName("panel");
    auto* aboutLayout = new QVBoxLayout(aboutCard);
    aboutLayout->setContentsMargins(20, 18, 20, 18);
    aboutLayout->setSpacing(10);
    auto* aboutTitle = new QLabel("About", aboutCard); aboutTitle->setObjectName("sectionTitle");
    auto* versionLabel = new QLabel(QString("%1  •  version %2").arg(POS_APP_NAME, POS_VERSION), aboutCard);
    auto* dataPathLabel = new QLabel(QString("Data folder: %1").arg(pos::paths::appDataDir()), aboutCard);
    dataPathLabel->setObjectName("muted"); dataPathLabel->setWordWrap(true); dataPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* receiptsPathLabel = new QLabel(QString("Receipts folder: %1").arg(QDir::toNativeSeparators(pos::paths::receiptsDir())), aboutCard);
    receiptsPathLabel->setObjectName("muted"); receiptsPathLabel->setWordWrap(true); receiptsPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* openDataBtn = new QPushButton("Open data folder", aboutCard);
    auto* openReceiptsBtn = new QPushButton("Open receipts folder", aboutCard);
    auto* aboutBtns = new QHBoxLayout;
    aboutBtns->addWidget(openDataBtn); aboutBtns->addWidget(openReceiptsBtn); aboutBtns->addStretch();
    aboutLayout->addWidget(aboutTitle);
    aboutLayout->addWidget(versionLabel);
    aboutLayout->addWidget(dataPathLabel);
    aboutLayout->addWidget(receiptsPathLabel);
    aboutLayout->addLayout(aboutBtns);
    layout->addWidget(aboutCard);

    connect(openDataBtn, &QPushButton::clicked, this, [this] {
        // PIN-protected: the data folder holds the raw database.
        if (!pos::authorizeSensitiveAction(this, database_, "open the data folder")) return;
        QDesktopServices::openUrl(QUrl::fromLocalFile(pos::paths::appDataDir()));
    });
    connect(openReceiptsBtn, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(pos::paths::receiptsDir()));
    });

    layout->addStretch();

    QTimer::singleShot(0, this, [this] { load(); });

    connect(saveSettingsBtn_, &QPushButton::clicked, this, [this] {
        try {
            pos::SettingsService service(database_);
            service.setValue("business.name", businessNameInput_->text().trimmed());
            service.setValue("business.phone", phoneInput_->text().trimmed());
            service.setValue("business.currency", currencyInput_->text().trimmed().isEmpty() ? "PKR" : currencyInput_->text().trimmed());
            service.setValue("receipt.footer", footerInput_->text());
            service.setValue("backup.interval_hours", QString::number(backupHoursSpin_->value()));
            const auto path = thermalPathInput_->text().trimmed();
            const auto mode = printerModeCombo_->currentData().toString();
            // Warn (but allow) if raw ESC/POS is aimed at a page/PDF printer — that is
            // exactly what produces broken "POS Receipt" files instead of PDFs.
            if (mode == "raw" && pos::ui::isLikelyNonThermal(path)) {
                const auto choice = QMessageBox::warning(this, "Raw mode for a non-thermal printer",
                    QString("\"%1\" looks like a PDF/XPS or office printer, not a thermal receipt printer. "
                            "Raw ESC/POS mode will produce an invalid file, not a printed page or PDF.\n\n"
                            "Switch to \"Normal printer (Windows driver)\" mode?").arg(path),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                if (choice == QMessageBox::Yes) { printerModeCombo_->setCurrentIndex(printerModeCombo_->findData("normal")); }
            }
            service.setValue("printer.thermal_path", path);
            service.setValue("printer.mode", printerModeCombo_->currentData().toString());
            service.setValue("printer.paper_mm", paperSizeCombo_->currentData().toString());
            service.setValue("backup.extra_dir", extraBackupInput_->text().trimmed());
            QMessageBox::information(this, "Settings saved", "Settings were saved to the local database. Restart the app to apply a changed automatic backup interval.");
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not save settings", error.what());
        }
    });

    connect(testReceiptBtn_, &QPushButton::clicked, this, [this] {
        try {
            pos::SettingsService s(database_);
            pos::ReceiptData data;
            data.storeName = s.value("business.name", "Invento");
            data.address = s.value("business.address");
            data.phone = s.value("business.phone");
            data.invoiceNo = "TEST-1";
            data.dateTime = QDateTime::currentDateTime().toString("d MMM yyyy, h:mm AP");
            data.cashier = s.value("receipt.cashier", "Owner");
            data.note = s.value("receipt.footer");
            data.lines = {{"Printer test item", 1, 10000, 0, 10000}};
            data.totalQty = 1;
            data.gross = 10000; data.netTotal = 10000; data.cashReceived = 10000;
            data.amountInWords = pos::ReceiptService::amountToWords(10000);
            const auto status = pos::ui::deliverReceipt(database_, data);
            QMessageBox::information(this, "Test receipt sent", status);
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not print receipt", error.what());
        }
    });

    connect(testLabelBtn_, &QPushButton::clicked, this, [this] {
        try {
            const auto path = pos::SettingsService(database_).value("printer.thermal_path");
            pos::ThermalPrintService::writeRaw(path, pos::ThermalPrintService::barcodeLabelBytes("Invento test label", "123456789012"));
            QMessageBox::information(this, "Label sent", "The Code128 test label was sent to the configured device.");
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not print label", error.what());
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

    setTabOrder(businessNameInput_, phoneInput_);
    setTabOrder(phoneInput_, currencyInput_);
    setTabOrder(currencyInput_, footerInput_);
    setTabOrder(footerInput_, backupHoursSpin_);
    setTabOrder(backupHoursSpin_, thermalPathInput_);
    setTabOrder(thermalPathInput_, printerModeCombo_);
    setTabOrder(printerModeCombo_, saveSettingsBtn_);
    setTabOrder(saveSettingsBtn_, viewNotificationsBtn_);
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
        printerModeCombo_->setCurrentIndex(qMax(0, printerModeCombo_->findData(service.value("printer.mode", "raw"))));
        paperSizeCombo_->setCurrentIndex(qMax(0, paperSizeCombo_->findData(service.value("printer.paper_mm", "80"))));
        extraBackupInput_->setText(service.value("backup.extra_dir"));

        const bool pinConfigured = pos::SecurityService(database_).hasPin();
        pinStatusLabel_->setText(pinConfigured
            ? "Owner PIN: configured. Change it from Commission Settings (needs the current PIN)."
            : "Owner PIN: not configured. It is set up when the app first starts.");
    } catch (...) {}
}
