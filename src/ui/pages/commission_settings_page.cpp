#include "ui/pages/commission_settings_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/partner_service.h"
#include "core/auth_session.h"
#include "core/security_service.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QTimer>

namespace {
// Live preview so the owner sees the split on a round number as they type.
QString previewText(double totalPct, double partnerPct) {
    const double sample = 10000;
    const double partner = sample * partnerPct / 100.0;
    const double mine = sample * (totalPct - partnerPct) / 100.0;
    return QString("On a sale of 10,000: Partner gets %1 · You get %2")
        .arg(partner, 0, 'f', 0).arg(mine, 0, 'f', 0);
}

bool editConfigDialog(QWidget* parent, std::shared_ptr<pos::Database> database, const QString& title,
                      const QList<pos::Partner>& partners, pos::CommissionConfig& config) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(440);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;

    auto* totalSpin = new QDoubleSpinBox(&dialog);
    totalSpin->setRange(0, 100); totalSpin->setDecimals(2); totalSpin->setSuffix(" %");
    totalSpin->setValue(config.totalBp / 100.0);
    auto* partnerSpin = new QDoubleSpinBox(&dialog);
    partnerSpin->setRange(0, 100); partnerSpin->setDecimals(2); partnerSpin->setSuffix(" %");
    partnerSpin->setValue(config.partnerBp / 100.0);
    auto* partnerCombo = new QComboBox(&dialog);
    partnerCombo->addItem("None", QString());
    for (const auto& p : partners) partnerCombo->addItem(p.name, p.id);
    const int idx = partnerCombo->findData(config.partnerId);
    if (idx >= 0) partnerCombo->setCurrentIndex(idx);

    form->addRow("Total %", totalSpin);
    form->addRow("Partner %", partnerSpin);
    form->addRow("Linked partner", partnerCombo);
    layout->addLayout(form);

    auto* preview = new QLabel(&dialog);
    preview->setObjectName("muted");
    preview->setWordWrap(true);
    layout->addWidget(preview);
    const auto refresh = [=] { preview->setText(previewText(totalSpin->value(), partnerSpin->value())); };
    QObject::connect(totalSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), &dialog, [refresh](double){ refresh(); });
    QObject::connect(partnerSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), &dialog, [refresh](double){ refresh(); });
    refresh();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        // Mirror the backend validation so the owner gets an inline message.
        const auto total = qRound(totalSpin->value() * 100);
        const auto partner = qRound(partnerSpin->value() * 100);
        if (partner > total) { QMessageBox::warning(&dialog, "Invalid", "Partner % cannot be greater than total %."); return; }
        if (partner > 0 && partnerCombo->currentData().toString().isEmpty()) { QMessageBox::warning(&dialog, "Invalid", "Select a partner when the partner % is greater than 0."); return; }
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) return false;

    config.totalBp = qRound(totalSpin->value() * 100);
    config.partnerBp = qRound(partnerSpin->value() * 100);
    config.partnerId = partnerCombo->currentData().toString();
    return true;
}
} // namespace

CommissionSettingsPage::CommissionSettingsPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(std::move(database)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel("Commission Settings", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    auto* subtitle = new QLabel("Owner-only. Set the total %, partner % and linked partner for every course and book. Cashiers cannot open this section.", this);
    subtitle->setObjectName("muted");
    layout->addWidget(subtitle);

    lockedLabel_ = new QLabel("This section is locked. Return and re-open it, entering the owner PIN, to view commission settings.", this);
    lockedLabel_->setObjectName("muted");
    lockedLabel_->setWordWrap(true);
    lockedLabel_->hide();
    layout->addWidget(lockedLabel_);

    auto* controls = new QHBoxLayout;
    auto* searchLabel = new QLabel("Find course or book:", this);
    search_ = new QLineEdit(this);
    search_->setPlaceholderText("Search by name...");
    search_->setMinimumWidth(280);
    searchLabel->setBuddy(search_);
    editBtn_ = new QPushButton("&Edit commission", this);
    editBtn_->setObjectName("primary");
    changePinBtn_ = new QPushButton("Change owner PIN", this);
    refreshBtn_ = new QPushButton("&Refresh", this);
    controls->addWidget(searchLabel);
    controls->addWidget(search_);
    controls->addStretch();
    controls->addWidget(editBtn_);
    controls->addWidget(changePinBtn_);
    controls->addWidget(refreshBtn_);
    layout->addLayout(controls);

    auto* panel = new QFrame(this);
    panel->setObjectName("panel");
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(16, 16, 16, 16);
    table_ = new QTableWidget(panel);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"Type", "Name", "Total %", "Partner %", "Partner"});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    panelLayout->addWidget(table_, 1);
    layout->addWidget(panel, 1);

    feedbackLabel_ = new QLabel(this);
    feedbackLabel_->setObjectName("muted");
    layout->addWidget(feedbackLabel_);

    connect(search_, &QLineEdit::textChanged, this, [this] { load(); });
    connect(refreshBtn_, &QPushButton::clicked, this, [this] { load(); });
    connect(editBtn_, &QPushButton::clicked, this, [this] { editSelected(); });
    connect(table_, &QTableWidget::activated, this, [this](const QModelIndex&) { editSelected(); });
    connect(changePinBtn_, &QPushButton::clicked, this, [this] {
        if (!ensureUnlocked()) return;
        bool ok = false;
        const auto pin = QInputDialog::getText(this, "Change owner PIN", "New PIN (4-12 digits):", QLineEdit::Password, {}, &ok);
        if (!ok) return;
        const auto confirm = QInputDialog::getText(this, "Change owner PIN", "Re-enter the new PIN:", QLineEdit::Password, {}, &ok);
        if (!ok) return;
        if (pin != confirm) { QMessageBox::warning(this, "PIN not changed", "The two entries did not match."); return; }
        try { pos::SecurityService(database_).setPin(pin); feedbackLabel_->setText("Owner PIN updated."); }
        catch (const std::exception& e) { QMessageBox::critical(this, "PIN not changed", e.what()); }
    });

    QTimer::singleShot(0, this, [this] { load(); });
}

bool CommissionSettingsPage::ensureUnlocked() {
    return pos::unlockOwnerSession(this, database_);
}

void CommissionSettingsPage::load() {
    auto& session = pos::AuthSession::instance();
    const bool unlocked = session.isUnlocked();
    lockedLabel_->setVisible(!unlocked);
    table_->setVisible(unlocked);
    editBtn_->setEnabled(unlocked);
    if (!unlocked) { table_->setRowCount(0); return; }
    session.touch();
    table_->setRowCount(0);
    try {
        const auto rows = pos::PartnerService(database_).listCommissionConfigs(search_->text());
        for (const auto& row : rows) {
            const int r = table_->rowCount();
            table_->insertRow(r);
            auto* typeItem = new QTableWidgetItem(row.kind == "course" ? "Course" : "Book");
            typeItem->setData(Qt::UserRole, row.id);
            typeItem->setData(Qt::UserRole + 1, row.kind);
            table_->setItem(r, 0, typeItem);
            table_->setItem(r, 1, new QTableWidgetItem(row.name));
            table_->setItem(r, 2, new QTableWidgetItem(QString::number(row.totalBp / 100.0, 'f', 2)));
            table_->setItem(r, 3, new QTableWidgetItem(QString::number(row.partnerBp / 100.0, 'f', 2)));
            table_->setItem(r, 4, new QTableWidgetItem(row.partnerName.isEmpty() ? "—" : row.partnerName));
        }
    } catch (const std::exception&) {
        lockedLabel_->show();
        table_->hide();
    }
}

void CommissionSettingsPage::editSelected() {
    if (!ensureUnlocked()) return;
    const int row = table_->currentRow();
    if (row < 0) { QMessageBox::information(this, "Edit commission", "Select a course or book first."); return; }
    const auto id = table_->item(row, 0)->data(Qt::UserRole).toString();
    const auto kind = table_->item(row, 0)->data(Qt::UserRole + 1).toString();
    const auto name = table_->item(row, 1)->text();
    try {
        pos::PartnerService service(database_);
        const auto partners = service.listPartners();
        auto config = kind == "course" ? service.courseConfig(id) : service.bookConfig(id);
        if (!editConfigDialog(this, database_, QString("Commission — %1").arg(name), partners, config)) return;
        if (kind == "course") service.setCourseConfig(id, config); else service.setBookConfig(id, config);
        feedbackLabel_->setText(QString("Commission updated for \"%1\".").arg(name));
        load();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Could not save", e.what());
    }
}
