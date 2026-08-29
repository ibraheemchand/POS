#include "ui/pages/cash_management_page.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/shift_service.h"
#include "core/data_change_bus.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>

CashManagementPage::CashManagementPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(database) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* title = new QLabel("Cash Management", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("sectionTitle");

    openBtn_ = new QPushButton("Open shift", this);
    openBtn_->setObjectName("primary");
    
    closeBtn_ = new QPushButton("Close shift", this);
    closeBtn_->setObjectName("danger");

    auto* actions = new QHBoxLayout;
    actions->addWidget(openBtn_);
    actions->addWidget(closeBtn_);
    actions->addStretch();
    layout->addLayout(actions);

    layout->addWidget(statusLabel_);
    layout->addStretch();

    // Initial setups
    QTimer::singleShot(0, this, [this] { load(); });

    // Connections
    connect(openBtn_, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto opening = QInputDialog::getInt(this, "Open shift", "Opening cash (paisa):", 0, 0, 1000000000, 1, &ok);
        if (!ok) return;
        try {
            pos::ShiftService(database_).open(opening);
            load();
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not open shift", error.what());
        }
    });

    connect(closeBtn_, &QPushButton::clicked, this, [this] {
        const auto id = pos::activeShiftId(*database_);
        if (id.isEmpty()) {
            QMessageBox::information(this, "No open shift", "Open a shift before closing the till.");
            return;
        }
        if (!pos::authorizeSensitiveAction(this, database_, "close the shift")) return;
        bool ok = false;
        const auto counted = QInputDialog::getInt(this, "Close shift", "Counted cash (paisa):", 0, 0, 1000000000, 1, &ok);
        if (!ok) return;
        try {
            const auto result = pos::ShiftService(database_).close(id, counted);
            load();
            QMessageBox::information(this, "Shift closed", QString("Expected: %1 paisa\nDifference: %2 paisa").arg(result.expected).arg(result.difference));
        } catch (const std::exception& error) {
            QMessageBox::critical(this, "Could not close shift", error.what());
        }
    });

    // Auto update wiring
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::cashChanged, this, &CashManagementPage::load);

    // Set tab order
    setTabOrder(openBtn_, closeBtn_);
}

void CashManagementPage::load() {
    try {
        const auto id = pos::activeShiftId(*database_);
        statusLabel_->setText(id.isEmpty() ? "Till status: CLOSED" : "Till status: OPEN\nShift: " + id);
    } catch (...) {}
}
