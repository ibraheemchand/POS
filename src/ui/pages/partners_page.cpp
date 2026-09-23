#include "ui/pages/partners_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/partner_service.h"
#include "core/auth_session.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QInputDialog>
#include <QTimer>

namespace {
bool editPartnerDialog(QWidget* parent, const QString& title, pos::Partner& partner) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setMinimumWidth(420);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout;
    auto* nameEdit = new QLineEdit(partner.name, &dialog);
    auto* phoneEdit = new QLineEdit(partner.phone, &dialog);
    auto* notesEdit = new QLineEdit(partner.notes, &dialog);
    form->addRow("Name", nameEdit);
    form->addRow("Phone", phoneEdit);
    form->addRow("Notes", notesEdit);
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return false;
    partner.name = nameEdit->text();
    partner.phone = phoneEdit->text();
    partner.notes = notesEdit->text();
    return true;
}
} // namespace

PartnersPage::PartnersPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(std::move(database)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel("Partners", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    auto* subtitle = new QLabel("Owner-only. Partner balances, ledgers and payouts.", this);
    subtitle->setObjectName("muted");
    layout->addWidget(subtitle);

    lockedLabel_ = new QLabel("This section is locked. Re-open it with the owner PIN to view partners.", this);
    lockedLabel_->setObjectName("muted");
    lockedLabel_->setWordWrap(true);
    lockedLabel_->hide();
    layout->addWidget(lockedLabel_);

    auto* controls = new QHBoxLayout;
    addBtn_ = new QPushButton("&Add partner", this);
    addBtn_->setObjectName("primary");
    editBtn_ = new QPushButton("&Edit", this);
    archiveBtn_ = new QPushButton("Ar&chive", this);
    archiveBtn_->setObjectName("danger");
    payoutBtn_ = new QPushButton("Record &payout", this);
    refreshBtn_ = new QPushButton("&Refresh", this);
    controls->addWidget(addBtn_);
    controls->addWidget(editBtn_);
    controls->addWidget(archiveBtn_);
    controls->addStretch();
    controls->addWidget(payoutBtn_);
    controls->addWidget(refreshBtn_);
    layout->addLayout(controls);

    auto* split = new QHBoxLayout;

    auto* partnersPanel = new QFrame(this);
    partnersPanel->setObjectName("panel");
    auto* partnersLayout = new QVBoxLayout(partnersPanel);
    partnersLayout->setContentsMargins(16, 16, 16, 16);
    auto* partnersTitle = new QLabel("Partners & balances", partnersPanel);
    partnersTitle->setObjectName("sectionTitle");
    partnersLayout->addWidget(partnersTitle);
    partnersTable_ = new QTableWidget(partnersPanel);
    partnersTable_->setColumnCount(3);
    partnersTable_->setHorizontalHeaderLabels({"Name", "Phone", "Balance (owed)"});
    partnersTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    partnersTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    partnersTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    partnersTable_->setAlternatingRowColors(true);
    partnersLayout->addWidget(partnersTable_, 1);
    split->addWidget(partnersPanel, 1);

    auto* ledgerPanel = new QFrame(this);
    ledgerPanel->setObjectName("panel");
    auto* ledgerLayout = new QVBoxLayout(ledgerPanel);
    ledgerLayout->setContentsMargins(16, 16, 16, 16);
    auto* ledgerTitle = new QLabel("Ledger", ledgerPanel);
    ledgerTitle->setObjectName("sectionTitle");
    ledgerLayout->addWidget(ledgerTitle);
    auto* dateRow = new QHBoxLayout;
    fromDate_ = new QDateEdit(QDate::currentDate().addMonths(-1), ledgerPanel);
    toDate_ = new QDateEdit(QDate::currentDate(), ledgerPanel);
    fromDate_->setCalendarPopup(true);
    toDate_->setCalendarPopup(true);
    dateRow->addWidget(new QLabel("From", ledgerPanel));
    dateRow->addWidget(fromDate_);
    dateRow->addWidget(new QLabel("To", ledgerPanel));
    dateRow->addWidget(toDate_);
    dateRow->addStretch();
    ledgerLayout->addLayout(dateRow);
    ledgerTable_ = new QTableWidget(ledgerPanel);
    ledgerTable_->setColumnCount(5);
    ledgerTable_->setHorizontalHeaderLabels({"Date", "Description", "Credit", "Debit", "Balance"});
    ledgerTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ledgerTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ledgerTable_->setAlternatingRowColors(true);
    ledgerLayout->addWidget(ledgerTable_, 1);
    split->addWidget(ledgerPanel, 1);

    layout->addLayout(split, 1);

    feedbackLabel_ = new QLabel(this);
    feedbackLabel_->setObjectName("muted");
    layout->addWidget(feedbackLabel_);

    connect(refreshBtn_, &QPushButton::clicked, this, [this] { load(); });
    connect(partnersTable_, &QTableWidget::itemSelectionChanged, this, [this] { loadLedger(); });
    connect(fromDate_, &QDateEdit::dateChanged, this, [this] { loadLedger(); });
    connect(toDate_, &QDateEdit::dateChanged, this, [this] { loadLedger(); });

    connect(addBtn_, &QPushButton::clicked, this, [this] {
        if (!ensureUnlocked()) return;
        pos::Partner partner;
        if (!editPartnerDialog(this, "New partner", partner)) return;
        try { pos::PartnerService(database_).createPartner(partner); feedbackLabel_->setText("Partner added."); load(); }
        catch (const std::exception& e) { QMessageBox::critical(this, "Could not add partner", e.what()); }
    });
    connect(editBtn_, &QPushButton::clicked, this, [this] {
        if (!ensureUnlocked()) return;
        const auto id = selectedPartnerId();
        if (id.isEmpty()) { QMessageBox::information(this, "Edit partner", "Select a partner first."); return; }
        try {
            pos::PartnerService service(database_);
            pos::Partner partner;
            for (const auto& p : service.listPartners(true)) if (p.id == id) { partner = p; break; }
            if (!editPartnerDialog(this, "Edit partner", partner)) return;
            service.updatePartner(partner);
            feedbackLabel_->setText("Partner updated.");
            load();
        } catch (const std::exception& e) { QMessageBox::critical(this, "Could not edit partner", e.what()); }
    });
    connect(archiveBtn_, &QPushButton::clicked, this, [this] {
        if (!ensureUnlocked()) return;
        const auto id = selectedPartnerId();
        if (id.isEmpty()) { QMessageBox::information(this, "Archive partner", "Select a partner first."); return; }
        if (QMessageBox::question(this, "Archive partner", "Archive this partner? Their history is kept.") != QMessageBox::Yes) return;
        try { pos::PartnerService(database_).archivePartner(id); feedbackLabel_->setText("Partner archived."); load(); }
        catch (const std::exception& e) { QMessageBox::critical(this, "Could not archive partner", e.what()); }
    });
    connect(payoutBtn_, &QPushButton::clicked, this, [this] {
        if (!ensureUnlocked()) return;
        const auto id = selectedPartnerId();
        if (id.isEmpty()) { QMessageBox::information(this, "Record payout", "Select a partner first."); return; }
        bool ok = false;
        const auto rupees = QInputDialog::getDouble(this, "Record payout", "Amount paid to the partner (PKR):", 0, 0, 1e9, 2, &ok);
        if (!ok || rupees <= 0) return;
        const auto note = QInputDialog::getText(this, "Record payout", "Note (optional):");
        try { pos::PartnerService(database_).recordPayout(id, static_cast<pos::Money>(qRound64(rupees * 100)), note); feedbackLabel_->setText("Payout recorded."); load(); }
        catch (const std::exception& e) { QMessageBox::critical(this, "Could not record payout", e.what()); }
    });

    QTimer::singleShot(0, this, [this] { load(); });
}

bool PartnersPage::ensureUnlocked() { return pos::unlockOwnerSession(this, database_); }

QString PartnersPage::selectedPartnerId() const {
    const int row = partnersTable_->currentRow();
    if (row < 0 || !partnersTable_->item(row, 0)) return {};
    return partnersTable_->item(row, 0)->data(Qt::UserRole).toString();
}

void PartnersPage::load() {
    auto& session = pos::AuthSession::instance();
    const bool unlocked = session.isUnlocked();
    lockedLabel_->setVisible(!unlocked);
    partnersTable_->setVisible(unlocked);
    ledgerTable_->setVisible(unlocked);
    for (auto* b : {addBtn_, editBtn_, archiveBtn_, payoutBtn_}) b->setEnabled(unlocked);
    if (!unlocked) { partnersTable_->setRowCount(0); ledgerTable_->setRowCount(0); return; }
    session.touch();
    partnersTable_->setRowCount(0);
    try {
        for (const auto& p : pos::PartnerService(database_).listPartners()) {
            const int r = partnersTable_->rowCount();
            partnersTable_->insertRow(r);
            auto* nameItem = new QTableWidgetItem(p.name);
            nameItem->setData(Qt::UserRole, p.id);
            partnersTable_->setItem(r, 0, nameItem);
            partnersTable_->setItem(r, 1, new QTableWidgetItem(p.phone));
            partnersTable_->setItem(r, 2, new QTableWidgetItem("PKR " + pos::formatPaisa(p.balance)));
        }
    } catch (const std::exception&) { lockedLabel_->show(); partnersTable_->hide(); return; }
    if (partnersTable_->rowCount() > 0) partnersTable_->selectRow(0);
    loadLedger();
}

void PartnersPage::loadLedger() {
    ledgerTable_->setRowCount(0);
    if (!pos::AuthSession::instance().isUnlocked()) return;
    const auto id = selectedPartnerId();
    if (id.isEmpty()) return;
    try {
        const auto rows = pos::PartnerService(database_).partnerLedger(id, fromDate_->date(), toDate_->date());
        for (const auto& row : rows) {
            const int r = ledgerTable_->rowCount();
            ledgerTable_->insertRow(r);
            ledgerTable_->setItem(r, 0, new QTableWidgetItem(row.createdAt.left(10)));
            const auto desc = row.invoiceNo.isEmpty() ? row.description : QString("%1 [%2]").arg(row.description, row.invoiceNo);
            ledgerTable_->setItem(r, 1, new QTableWidgetItem(desc));
            ledgerTable_->setItem(r, 2, new QTableWidgetItem(row.credit ? "PKR " + pos::formatPaisa(row.credit) : ""));
            ledgerTable_->setItem(r, 3, new QTableWidgetItem(row.debit ? "PKR " + pos::formatPaisa(row.debit) : ""));
            ledgerTable_->setItem(r, 4, new QTableWidgetItem("PKR " + pos::formatPaisa(row.balance)));
        }
    } catch (const std::exception&) {}
}
