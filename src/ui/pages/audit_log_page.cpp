#include "ui/pages/audit_log_page.h"
#include "core/database.h"
#include "core/audit_service.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QHeaderView>
#include <QTimer>

AuditLogPage::AuditLogPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(database) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* title = new QLabel("Audit Log", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    auto* actions = new QHBoxLayout;
    filterInput_ = new QLineEdit(this);
    filterInput_->setPlaceholderText("Filter by action (optional)");
    
    refreshBtn_ = new QPushButton("Refresh", this);
    
    actions->addWidget(filterInput_, 1);
    actions->addWidget(refreshBtn_);
    layout->addLayout(actions);

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({"Action", "Entity", "Entity ID", "Detail", "Created"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_, 1);

    // Initial setups
    QTimer::singleShot(0, this, [this] { load(); });

    // Connections
    connect(refreshBtn_, &QPushButton::clicked, this, [this] { load(); });
    connect(filterInput_, &QLineEdit::textChanged, this, [this] { load(); });

    // Set tab order
    setTabOrder(filterInput_, refreshBtn_);
    setTabOrder(refreshBtn_, table_);
}

void AuditLogPage::load() {
    try {
        const auto rows = pos::AuditService(database_).recent(filterInput_->text().trimmed());
        table_->setRowCount(0);
        for (const auto& entry : rows) {
            const int row = table_->rowCount();
            table_->insertRow(row);
            table_->setItem(row, 0, new QTableWidgetItem(entry.action));
            table_->setItem(row, 1, new QTableWidgetItem(entry.entityType));
            table_->setItem(row, 2, new QTableWidgetItem(entry.entityId));
            table_->setItem(row, 3, new QTableWidgetItem(entry.detail));
            table_->setItem(row, 4, new QTableWidgetItem(entry.createdAt));
        }
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Could not load audit log", error.what());
    }
}
