#include "ui/pages/main_page.h"
#include "ui/pages/nav_catalog.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/report_service.h"
#include "core/inventory_service.h"
#include "core/customer_service.h"
#include "core/data_change_bus.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QPushButton>
#include <QColor>
#include <QIcon>
#include <QResizeEvent>
#include <QTime>
#include <QDate>

MainPage::MainPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(std::move(database)) {
    auto* l = new QVBoxLayout(this);
    l->setContentsMargins(0, 4, 0, 0);
    l->setSpacing(10);

    auto* heading = new QHBoxLayout;
    auto* title = new QLabel(this);
    title->setObjectName("pageTitle");
    const auto hour = QTime::currentTime().hour();
    title->setText(hour < 12 ? "Good morning" : hour < 17 ? "Good afternoon" : "Good evening");

    auto* intro = new QLabel("Here's a live view of your wholesale operation.", this);
    intro->setObjectName("muted");
    auto* tx = new QVBoxLayout;
    tx->addWidget(title);
    tx->addWidget(intro);
    heading->addLayout(tx);
    heading->addStretch();

    l->addLayout(heading);

    metricValues_.resize(6);
    metricCaptions_.resize(6);
    metricsGrid_ = new QGridLayout;
    metricsGrid_->setHorizontalSpacing(10);
    metricsGrid_->setVerticalSpacing(8);

    const QList<QString> accents = {"#2563EB", "#22C55E", "#F59E0B", "#EF4444", "#7C3AED", "#EA580C"};
    const QList<QString> labels = {"Today's sales", "Today's cash", "Receivables", "Low stock", "Today's purchases", "Expiring batches"};

    for (int i = 0; i < 6; ++i) {
        auto* card = new QFrame(this);
        card->setObjectName("metric");
        // No fixed height: the card grows to fit its (DPI-scaled) text. A minimum
        // width keeps it readable while the grid reflows the column count.
        card->setMinimumWidth(160);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(3);

        auto* label = new QLabel(labels[i], card);
        label->setObjectName("metricLabel");
        label->setWordWrap(true);
        label->setStyleSheet("font-weight: 700; text-transform: uppercase; letter-spacing: 0.5px;");

        auto* value = new QLabel("PKR 0.00", card);
        value->setObjectName("metricValue");
        value->setWordWrap(true);
        value->setStyleSheet("color:" + accents[i] + "; font-weight: 800; font-family: 'JetBrains Mono', 'Segoe UI', monospace;");

        auto* caption = new QLabel("Updated live", card);
        caption->setObjectName("metricCaption");
        caption->setWordWrap(true);

        cardLayout->addWidget(label);
        cardLayout->addWidget(value);
        cardLayout->addWidget(caption);

        metricValues_[i] = value;
        metricCaptions_[i] = caption;
        metricCards_.append(card);
    }
    l->addLayout(metricsGrid_);

    // --- Quick Access ---
    auto* quickPanel = new QFrame(this);
    quickPanel->setObjectName("panel");
    auto* ql = new QVBoxLayout(quickPanel);
    ql->setContentsMargins(14, 10, 14, 12);
    ql->setSpacing(8);
    auto* quickTitle = new QLabel("Quick Access", quickPanel);
    quickTitle->setObjectName("sectionTitle");
    ql->addWidget(quickTitle);

    quickGrid_ = new QGridLayout;
    quickGrid_->setHorizontalSpacing(10);
    quickGrid_->setVerticalSpacing(8);
    for (const auto& entry : pos::navCatalog()) {
        if (entry.name == "Main") continue; // no self-link; you're already here
        auto* btn = new QPushButton(quickPanel);
        btn->setObjectName("quickCard");
        btn->setIcon(QIcon(entry.iconPath));
        btn->setIconSize(QSize(22, 22));
        btn->setCursor(Qt::PointingHandCursor);
        // Minimum height derives from the font (two text lines + padding) so it
        // scales with DPI and never clips the name or the shortcut line.
        btn->setMinimumHeight(fontMetrics().height() * 2 + 26);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
        // Name (+ lock for owner pages) on the first line, shortcut on the second.
        const auto lock = entry.gated ? QString("  \xF0\x9F\x94\x92") : QString();
        btn->setText(entry.name + lock + "\n" + entry.shortcut);
        const auto target = entry.name;
        connect(btn, &QPushButton::clicked, this, [this, target] { emit requestNavigation(target); });
        quickButtons_.append(btn);
    }
    ql->addLayout(quickGrid_);
    l->addWidget(quickPanel);
    relayoutQuickAccess();

    auto* lower = new QHBoxLayout;
    lower->setSpacing(10);

    auto* activity = new QFrame(this);
    activity->setObjectName("panel");
    activity->setMinimumHeight(110);
    auto* al = new QVBoxLayout(activity);
    al->setContentsMargins(14, 10, 14, 10);
    al->setSpacing(6);
    auto* at = new QLabel("Recent activity", activity);
    at->setObjectName("sectionTitle");
    al->addWidget(at);

    recent_ = new QListWidget(activity);
    recent_->setObjectName("recentList");
    recent_->setFocusPolicy(Qt::NoFocus);
    al->addWidget(recent_, 1);
    lower->addWidget(activity, 3);

    auto* lowStockAlerts = new QFrame(this);
    lowStockAlerts->setObjectName("panel");
    lowStockAlerts->setMinimumHeight(110);
    auto* nl = new QVBoxLayout(lowStockAlerts);
    nl->setContentsMargins(14, 10, 14, 10);
    nl->setSpacing(6);
    auto* nt = new QLabel("Low Stock Alerts", lowStockAlerts);
    nt->setObjectName("sectionTitle");
    nl->addWidget(nt);

    lowStockList_ = new QListWidget(lowStockAlerts);
    lowStockList_->setObjectName("recentList");
    lowStockList_->setFocusPolicy(Qt::NoFocus);
    nl->addWidget(lowStockList_, 1);
    lower->addWidget(lowStockAlerts, 2);

    l->addLayout(lower, 1);

    // Live update connections
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::salesChanged, this, &MainPage::load);
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::inventoryChanged, this, &MainPage::load);
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::cashChanged, this, &MainPage::load);
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::purchasesChanged, this, &MainPage::load);
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::customersChanged, this, &MainPage::load);
    connect(&pos::DataChangeBus::instance(), &pos::DataChangeBus::suppliersChanged, this, &MainPage::load);

    // Initial load
    load();

}

void MainPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    relayoutQuickAccess();
}

void MainPage::relayoutQuickAccess() {
    // Base the column count on the SCROLL VIEWPORT width (our parent), not our own
    // width(): inside a scroll area the widget's width inflates to its content's
    // minimum, which would feed back and prevent the grid from ever shrinking.
    const int avail = parentWidget() ? parentWidget()->width() : width();
    const int metricCols = qBound(2, avail / 190, 6);
    if (metricsGrid_ && !metricCards_.isEmpty() && metricCols != metricColumns_) {
        metricColumns_ = metricCols;
        for (auto* card : metricCards_) metricsGrid_->removeWidget(card);
        for (int i = 0; i < metricCards_.size(); ++i)
            metricsGrid_->addWidget(metricCards_[i], i / metricCols, i % metricCols);
        for (int c = 0; c < metricCols; ++c) metricsGrid_->setColumnStretch(c, 1);
    }

    const int quickCols = qBound(2, avail / 210, 6);
    if (quickGrid_ && !quickButtons_.isEmpty() && quickCols != quickColumns_) {
        quickColumns_ = quickCols;
        for (auto* btn : quickButtons_) quickGrid_->removeWidget(btn);
        for (int i = 0; i < quickButtons_.size(); ++i)
            quickGrid_->addWidget(quickButtons_[i], i / quickCols, i % quickCols);
        for (int c = 0; c < quickCols; ++c) quickGrid_->setColumnStretch(c, 1);
    }
}

void MainPage::load() {
    try {
        const auto today = QDate::currentDate();
        pos::ReportService reportService(database_);
        pos::InventoryService inventoryService(database_);
        pos::CustomerService customerService(database_);

        // 1. Today's sales
        try {
            const auto salesVal = reportService.summary(today, today).sales;
            metricValues_[0]->setText("PKR " + pos::formatMoney(salesVal));
            const auto count = reportService.salesCount(today);
            metricCaptions_[0]->setText(QString("%1 completed invoices").arg(count));
        } catch (...) {
            metricValues_[0]->setText("PKR 0.00");
            metricCaptions_[0]->setText("0 completed invoices");
        }

        // 2. Today's cash
        try {
            const auto cashVal = reportService.todayCashSales(today);
            metricValues_[1]->setText("PKR " + pos::formatMoney(cashVal));
            metricCaptions_[1]->setText("Cash payments received");
        } catch (...) {
            metricValues_[1]->setText("PKR 0.00");
            metricCaptions_[1]->setText("Cash payments received");
        }

        // 3. Receivables
        try {
            const auto recVal = customerService.totalReceivables();
            metricValues_[2]->setText("PKR " + pos::formatMoney(recVal));
            metricCaptions_[2]->setText("Unpaid customer balances");
        } catch (...) {
            metricValues_[2]->setText("PKR 0.00");
            metricCaptions_[2]->setText("Unpaid customer balances");
        }

        // 4. Low stock
        try {
            const auto lowStockAlertsList = inventoryService.lowStock();
            metricValues_[3]->setText(QString::number(lowStockAlertsList.size()));
            metricCaptions_[3]->setText("Products below threshold");
        } catch (...) {
            metricValues_[3]->setText("0");
            metricCaptions_[3]->setText("Products below threshold");
        }

        // 5. Today's purchases
        try {
            const auto purVal = reportService.summary(today, today).purchases;
            metricValues_[4]->setText("PKR " + pos::formatMoney(purVal));
            metricCaptions_[4]->setText("Received stock value");
        } catch (...) {
            metricValues_[4]->setText("PKR 0.00");
            metricCaptions_[4]->setText("Received stock value");
        }

        // 6. Expiring batches
        try {
            const auto expiringVal = reportService.expiringBatchesCount(today, today.addDays(30));
            metricValues_[5]->setText(QString::number(expiringVal));
            metricCaptions_[5]->setText("Batches expiring within 30 days");
        } catch (...) {
            metricValues_[5]->setText("0");
            metricCaptions_[5]->setText("Batches expiring within 30 days");
        }

        // Recent Activity
        try {
            recent_->clear();
            const auto sales = reportService.recentSales(4);
            for (const auto& item : sales) {
                recent_->addItem(QString("Sale %1  •  PKR %2  •  %3").arg(item.invoiceNo).arg(pos::formatMoney(item.total)).arg(item.date));
            }
            const auto purchases = reportService.recentPurchases(4);
            for (const auto& item : purchases) {
                recent_->addItem(QString("Purchase %1  •  PKR %2  •  %3").arg(item.invoiceNo).arg(pos::formatMoney(item.total)).arg(item.date));
            }
        } catch (...) {}

        // Low Stock list
        try {
            lowStockList_->clear();
            const auto lowStockItems = inventoryService.listLowStock(10);
            for (const auto& item : lowStockItems) {
                lowStockList_->addItem(QString("%1 — %2 / %3 %4").arg(item.name).arg(item.quantity).arg(item.minimumStock).arg(item.baseUnit));
            }
        } catch (...) {}
    } catch (...) {}
}