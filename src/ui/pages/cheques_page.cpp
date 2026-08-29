#include "ui/pages/cheques_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/cheque_service.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QTimer>

ChequesPage::ChequesPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(database) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* title = new QLabel("Cheques", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    auto* actions = new QHBoxLayout;
    addBtn_ = new QPushButton("Record cheque", this);
    addBtn_->setObjectName("primary");
    statusBtn_ = new QPushButton("Update status", this);
    refreshBtn_ = new QPushButton("Refresh", this);

    actions->addWidget(addBtn_);
    actions->addWidget(statusBtn_);
    actions->addWidget(refreshBtn_);
    actions->addStretch();
    layout->addLayout(actions);

    table_ = new QTableWidget(this);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({"Direction", "Cheque no.", "Bank", "Amount", "Due date", "Status"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_, 1);

    // Initial setups
    QTimer::singleShot(0, this, [this] { load(); });

    // Connections
    connect(refreshBtn_, &QPushButton::clicked, this, [this] { load(); });

    connect(addBtn_, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto direction = QInputDialog::getItem(this, "Record cheque", "Direction:", {"received", "issued"}, 0, false, &ok);
        if (!ok) return;
        const auto number = QInputDialog::getText(this, "Record cheque", "Cheque number:", QLineEdit::Normal, {}, &ok);
        if (!ok || number.trimmed().isEmpty()) return;
        const auto bank = QInputDialog::getText(this, "Record cheque", "Bank:", QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        const auto amount = QInputDialog::getInt(this, "Record cheque", "Amount (paisa):", 0, 1, 1000000000, 1, &ok);
        if (!ok) return;
        const auto due = QInputDialog::getText(this, "Record cheque", "Due date (YYYY-MM-DD):", QLineEdit::Normal, QDate::currentDate().toString(Qt::ISODate), &ok);
        if (!ok) return;
        const auto date = QDate::fromString(due, Qt::ISODate);
        if (!date.isValid()) {
            QMessageBox::warning(this, "Invalid date", "Enter a valid date in YYYY-MM-DD format.");
            return;
        }
        try {
            pos::ChequeService(database_).record({{}, direction, {}, number, bank, {}, amount, date});
            load();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not record cheque", error.what());
        }
    });

    connect(statusBtn_, &QPushButton::clicked, this, [this] {
        const auto row = table_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "Update cheque", "Select a cheque first.");
            return;
        }
        const auto id = table_->item(row, 1)->data(Qt::UserRole).toString();
        bool ok = false;
        const auto next = QInputDialog::getItem(this, "Update cheque status", "Status:", {"pending", "deposited", "cleared", "bounced"}, 0, false, &ok);
        if (!ok) return;
        try {
            pos::ChequeService(database_).setStatus(id, next);
            load();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not update cheque", error.what());
        }
    });

    // Set tab order
    setTabOrder(addBtn_, statusBtn_);
    setTabOrder(statusBtn_, refreshBtn_);
    setTabOrder(refreshBtn_, table_);
}

void ChequesPage::load() {
    table_->setRowCount(0);
    try {
        const auto list = pos::ChequeService(database_).listAll();
        for (const auto& item : list) {
            const int row = table_->rowCount();
            table_->insertRow(row);
            
            table_->setItem(row, 0, new QTableWidgetItem(item.direction));
            
            auto* noItem = new QTableWidgetItem(item.number);
            noItem->setData(Qt::UserRole, item.id);
            table_->setItem(row, 1, noItem);
            
            table_->setItem(row, 2, new QTableWidgetItem(item.bank));
            table_->setItem(row, 3, new QTableWidgetItem("PKR " + pos::formatPaisa(item.amount)));
            table_->setItem(row, 4, new QTableWidgetItem(item.dueDate.toString(Qt::ISODate)));
            table_->setItem(row, 5, new QTableWidgetItem(item.status));
        }
    } catch (...) {}
}
