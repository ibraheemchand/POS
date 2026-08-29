#include "ui/pages/suppliers_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/supplier_service.h"
#include "core/data_change_bus.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QTimer>

SuppliersPage::SuppliersPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(database) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* title = new QLabel("Suppliers", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    auto* actions = new QHBoxLayout;
    addBtn_ = new QPushButton("&Add supplier", this);
    addBtn_->setObjectName("primary");

    archiveBtn_ = new QPushButton("Ar&chive selected", this);
    archiveBtn_->setObjectName("danger");

    ledgerBtn_ = new QPushButton("View &ledger", this);
    refreshBtn_ = new QPushButton("&Refresh", this);

    actions->addWidget(addBtn_);
    actions->addWidget(archiveBtn_);
    actions->addWidget(ledgerBtn_);
    actions->addWidget(refreshBtn_);
    actions->addStretch();
    layout->addLayout(actions);

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"Supplier", "Contact", "Phone", "Address", "Payable"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_, 1);

    auto* ledgerTitle = new QLabel("Supplier ledger", this);
    ledgerTitle->setObjectName("sectionTitle");
    layout->addWidget(ledgerTitle);

    ledgerTable_ = new QTableWidget(this);
    ledgerTable_->setColumnCount(5);
    ledgerTable_->setHorizontalHeaderLabels({"Date", "Entry", "Reference", "Debit", "Credit"});
    ledgerTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ledgerTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ledgerTable_->setFixedHeight(170);
    layout->addWidget(ledgerTable_);

    // Initial setups
    QTimer::singleShot(0, this, [this] { load(); });

    // Connections
    connect(refreshBtn_, &QPushButton::clicked, this, [this] {
        load();
        loadLedger();
    });
    connect(table_, &QTableWidget::itemSelectionChanged, this, &SuppliersPage::loadLedger);
    connect(ledgerBtn_, &QPushButton::clicked, this, &SuppliersPage::loadLedger);

    connect(addBtn_, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto name = QInputDialog::getText(this, "New supplier", "Supplier name:", QLineEdit::Normal, {}, &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        const auto contact = QInputDialog::getText(this, "New supplier", "Contact person:", QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        const auto phone = QInputDialog::getText(this, "New supplier", "Phone:", QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        const auto address = QInputDialog::getText(this, "New supplier", "Address:", QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        const auto opening = QInputDialog::getInt(this, "New supplier", "Opening payable (paisa):", 0, 0, 1000000000, 1, &ok);
        if (!ok) return;

        try {
            pos::SupplierService(database_).create({{}, name, contact, phone, address, opening, false});
            load();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not add supplier", error.what());
        }
    });

    connect(archiveBtn_, &QPushButton::clicked, this, [this] {
        const auto row = table_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "Archive supplier", "Select a supplier first.");
            return;
        }
        const auto id = table_->item(row, 0)->data(Qt::UserRole).toString();
        if (QMessageBox::question(this, "Archive supplier", "Archive the selected supplier?") == QMessageBox::Yes) {
            if (!pos::authorizeSensitiveAction(this, database_, "archive a supplier")) return;
            try {
                pos::SupplierService(database_).archive(id);
                load();
                loadLedger();
            } catch (const std::exception& error) {
                QMessageBox::critical(this, "Could not archive supplier", error.what());
            }
        }
    });

    // Auto update wiring
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::suppliersChanged, this, [this] {
        load();
        loadLedger();
    });
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::purchasesChanged, this, [this] {
        load();
        loadLedger();
    });

    // Set tab order
    setTabOrder(addBtn_, archiveBtn_);
    setTabOrder(archiveBtn_, ledgerBtn_);
    setTabOrder(ledgerBtn_, refreshBtn_);
    setTabOrder(refreshBtn_, table_);
    setTabOrder(table_, ledgerTable_);
}

void SuppliersPage::load() {
    table_->setRowCount(0);
    try {
        const auto list = pos::SupplierService(database_).listActive();
        for (const auto& item : list) {
            const int row = table_->rowCount();
            table_->insertRow(row);
            
            auto* nameItem = new QTableWidgetItem(item.name);
            nameItem->setData(Qt::UserRole, item.id);
            table_->setItem(row, 0, nameItem);
            
            table_->setItem(row, 1, new QTableWidgetItem(item.contactPerson));
            table_->setItem(row, 2, new QTableWidgetItem(item.phone));
            table_->setItem(row, 3, new QTableWidgetItem(item.address));
            table_->setItem(row, 4, new QTableWidgetItem("PKR " + pos::formatPaisa(item.balance)));
        }
    } catch (...) {}
}

void SuppliersPage::loadLedger() {
    ledgerTable_->setRowCount(0);
    const auto row = table_->currentRow();
    if (row < 0) return;
    try {
        const auto entries = pos::SupplierService(database_).ledger(table_->item(row, 0)->data(Qt::UserRole).toString());
        for (const auto& entry : entries) {
            const int target = ledgerTable_->rowCount();
            ledgerTable_->insertRow(target);
            
            ledgerTable_->setItem(target, 0, new QTableWidgetItem(entry.createdAt));
            ledgerTable_->setItem(target, 1, new QTableWidgetItem(entry.entryType));
            ledgerTable_->setItem(target, 2, new QTableWidgetItem(entry.referenceId));
            ledgerTable_->setItem(target, 3, new QTableWidgetItem("PKR " + pos::formatPaisa(entry.debit)));
            ledgerTable_->setItem(target, 4, new QTableWidgetItem("PKR " + pos::formatPaisa(entry.credit)));
        }
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Could not load supplier ledger", error.what());
    }
}
