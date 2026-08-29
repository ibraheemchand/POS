#include "ui/pages/customers_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/customer_service.h"
#include "core/payment_service.h"
#include "core/data_change_bus.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QTimer>

CustomersPage::CustomersPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(database) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* title = new QLabel("Customers", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    auto* actions = new QHBoxLayout;
    addBtn_ = new QPushButton("&Add customer", this);
    addBtn_->setObjectName("primary");
    
    editBtn_ = new QPushButton("&Edit customer", this);
    
    paymentBtn_ = new QPushButton("Record &payment", this);
    refreshBtn_ = new QPushButton("&Refresh", this);

    actions->addWidget(addBtn_);
    actions->addWidget(editBtn_);
    actions->addWidget(paymentBtn_);
    actions->addWidget(refreshBtn_);
    actions->addStretch();
    layout->addLayout(actions);

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"Customer", "Phone", "Credit limit", "Outstanding", "Terms (days)"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    layout->addWidget(table_, 1);

    // Initial setups
    QTimer::singleShot(0, this, [this] { load(); });

    // Connections
    connect(refreshBtn_, &QPushButton::clicked, this, [this] { load(); });
    connect(table_, &QTableWidget::activated, this, &CustomersPage::editSelectedCustomer);
    connect(editBtn_, &QPushButton::clicked, this, &CustomersPage::editSelectedCustomer);

    connect(addBtn_, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto name = QInputDialog::getText(this, "New customer", "Customer name:", QLineEdit::Normal, {}, &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        const auto phone = QInputDialog::getText(this, "New customer", "Phone:", QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        const auto limit = QInputDialog::getInt(this, "New customer", "Credit limit (paisa):", 0, 0, 1000000000, 1, &ok);
        if (!ok) return;
        const auto terms = QInputDialog::getInt(this, "New customer", "Payment terms (days):", 0, 0, 365, 1, &ok);
        if (!ok) return;

        try {
            pos::CustomerService(database_).create({{}, name, phone, limit, terms, false});
            load();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not add customer", error.what());
        }
    });

    connect(paymentBtn_, &QPushButton::clicked, this, [this] {
        const auto row = table_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, "Record payment", "Select a customer first.");
            return;
        }
        const auto customerId = table_->item(row, 0)->data(Qt::UserRole).toString();
        
        pos::CustomerService service(database_);
        const auto unpaid = service.unpaidInvoices(customerId);
        if (unpaid.isEmpty()) {
            QMessageBox::information(this, "Record payment", "This customer has no outstanding invoices.");
            return;
        }

        QStringList choices;
        QList<QString> ids;
        for (const auto& inv : unpaid) {
            ids.append(inv.id);
            choices.append(inv.invoiceNo + " (due " + QString::number(inv.due) + " paisa)");
        }

        bool ok = false;
        const auto selected = QInputDialog::getItem(this, "Allocate payment", "Invoice:", choices, 0, false, &ok);
        if (!ok) return;
        const auto amount = QInputDialog::getInt(this, "Allocate payment", "Amount (paisa):", 0, 1, 1000000000, 1, &ok);
        if (!ok) return;
        const auto method = QInputDialog::getItem(this, "Allocate payment", "Method:", {"cash", "cheque", "mobile_wallet", "bank"}, 0, false, &ok);
        if (!ok) return;
        const auto index = choices.indexOf(selected);
        try {
            pos::PaymentService(database_).recordCustomerPayment(customerId, {{ids.at(index), amount}}, method, "Customer payment");
            load();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not record payment", error.what());
        }
    });

    // Auto update wiring
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::customersChanged, this, &CustomersPage::load);
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::salesChanged, this, &CustomersPage::load);

    // Set tab order
    setTabOrder(addBtn_, editBtn_);
    setTabOrder(editBtn_, paymentBtn_);
    setTabOrder(paymentBtn_, refreshBtn_);
    setTabOrder(refreshBtn_, table_);
}

void CustomersPage::load() {
    table_->setRowCount(0);
    try {
        pos::CustomerService service(database_);
        const auto list = service.listActive();
        for (const auto& item : list) {
            const int row = table_->rowCount();
            table_->insertRow(row);
            
            auto* nameItem = new QTableWidgetItem(item.name);
            nameItem->setData(Qt::UserRole, item.id);
            table_->setItem(row, 0, nameItem);
            
            table_->setItem(row, 1, new QTableWidgetItem(item.phone));
            table_->setItem(row, 2, new QTableWidgetItem("PKR " + pos::formatPaisa(item.creditLimit)));
            
            auto* outstanding = new QTableWidgetItem("PKR " + pos::formatPaisa(item.balance));
            outstanding->setForeground(item.balance > 0 ? QColor("#B3261E") : QColor("#16A34A"));
            table_->setItem(row, 3, outstanding);
            
            table_->setItem(row, 4, new QTableWidgetItem(QString::number(item.paymentTermsDays)));
        }
    } catch (...) {}
}

void CustomersPage::editSelectedCustomer() {
    const auto row = table_->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "Edit customer", "Select a customer first.");
        return;
    }
    const auto id = table_->item(row, 0)->data(Qt::UserRole).toString();
    const auto oldName = table_->item(row, 0)->text();
    const auto oldPhone = table_->item(row, 1)->text();
    // Get credit limit and terms from columns 2 and 4
    bool ok = false;
    const auto name = QInputDialog::getText(this, "Edit customer", "Customer name:", QLineEdit::Normal, oldName, &ok);
    if (!ok) return;
    const auto phone = QInputDialog::getText(this, "Edit customer", "Phone:", QLineEdit::Normal, oldPhone, &ok);
    if (!ok) return;
    
    try {
        // Find existing to get credit limit and terms
        pos::CustomerService service(database_);
        const auto active = service.listActive();
        pos::Customer target;
        for (const auto& c : active) {
            if (c.id == id) {
                target = c;
                break;
            }
        }
        
        const auto limit = QInputDialog::getInt(this, "Edit customer", "Credit limit (paisa):", target.creditLimit, 0, 1000000000, 1, &ok);
        if (!ok) return;
        const auto terms = QInputDialog::getInt(this, "Edit customer", "Payment terms (days):", target.paymentTermsDays, 0, 365, 1, &ok);
        if (!ok) return;

        target.name = name;
        target.phone = phone;
        target.creditLimit = limit;
        target.paymentTermsDays = terms;

        service.update(target);
        load();
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Could not edit customer", error.what());
    }
}
