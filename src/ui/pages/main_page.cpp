#include "ui/pages/main_page.h"
#include "ui/pages/sales_trend_graph.h"
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
    auto* metrics = new QGridLayout;
    metrics->setHorizontalSpacing(10);
    metrics->setVerticalSpacing(8);

    const QList<QString> accents = {"#2563EB", "#22C55E", "#F59E0B", "#EF4444", "#7C3AED", "#EA580C"};
    const QList<QString> labels = {"Today's sales", "Today's cash", "Receivables", "Low stock", "Today's purchases", "Expiring batches"};

    for (int i = 0; i < 6; ++i) {
        auto* card = new QFrame(this);
        card->setObjectName("metric");
        card->setMinimumHeight(84);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 8, 12, 8);
        cardLayout->setSpacing(2);

        auto* label = new QLabel(labels[i], card);
        label->setObjectName("metricLabel");
        label->setStyleSheet("font-size: 10.5px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.5px;");

        auto* value = new QLabel(i == 3 || i == 5 ? "PKR 0.00" : "PKR 0.00", card);
        value->setObjectName("metricValue");
        value->setStyleSheet("color:" + accents[i] + "; font-size: 18px; font-weight: 800; font-family: 'JetBrains Mono', 'Segoe UI', monospace;");

        auto* caption = new QLabel("Updated live", card);
        caption->setObjectName("metricCaption");
        caption->setStyleSheet("font-size: 10.5px;");

        cardLayout->addWidget(label);
        cardLayout->addWidget(value);
        cardLayout->addWidget(caption);

        metricValues_[i] = value;
        metricCaptions_[i] = caption;
        metrics->addWidget(card, i / 3, i % 3);
    }
    l->addLayout(metrics);

    // --- Trend chart ---
    auto* graphPanel = new QFrame(this);
    graphPanel->setObjectName("panel");
    graphPanel->setMinimumHeight(120);
    auto* gl = new QVBoxLayout(graphPanel);
    gl->setContentsMargins(14, 10, 14, 10);
    gl->setSpacing(4);

    chart_ = new SalesTrendGraph(graphPanel);
    gl->addWidget(chart_, 1);
    l->addWidget(graphPanel);

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

    // Set tab order
    // No quick actions to set tab order for
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
            metricValues_[0]->setText("PKR " + pos::formatPaisa(salesVal));
            const auto count = reportService.salesCount(today);
            metricCaptions_[0]->setText(QString("%1 completed invoices").arg(count));
        } catch (...) {
            metricValues_[0]->setText("PKR 0.00");
            metricCaptions_[0]->setText("0 completed invoices");
        }

        // 2. Today's cash
        try {
            const auto cashVal = reportService.todayCashSales(today);
            metricValues_[1]->setText("PKR " + pos::formatPaisa(cashVal));
            metricCaptions_[1]->setText("Cash payments received");
        } catch (...) {
            metricValues_[1]->setText("PKR 0.00");
            metricCaptions_[1]->setText("Cash payments received");
        }

        // 3. Receivables
        try {
            const auto recVal = customerService.totalReceivables();
            metricValues_[2]->setText("PKR " + pos::formatPaisa(recVal));
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
            metricValues_[4]->setText("PKR " + pos::formatPaisa(purVal));
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

        // Trend chart
        try {
            QVector<double> trendValues;
            QStringList trendLabels;
            const auto rawTrend = reportService.salesTrend(today.addDays(-6), today);
            QMap<QString, double> trendMap;
            for (const auto& pair : rawTrend) {
                trendMap[pair.first] = static_cast<double>(pair.second) / 100.0;
            }
            for (int d = -6; d <= 0; ++d) {
                const auto dt = today.addDays(d);
                const auto key = dt.toString(Qt::ISODate);
                trendValues.append(trendMap.value(key, 0.0));
                trendLabels.append(dt.toString("dd MMM"));
            }
            chart_->setData(trendValues, trendLabels);
        } catch (...) {}

        // Recent Activity
        try {
            recent_->clear();
            const auto sales = reportService.recentSales(4);
            for (const auto& item : sales) {
                recent_->addItem(QString("Sale %1  •  PKR %2  •  %3").arg(item.invoiceNo).arg(pos::formatPaisa(item.total)).arg(item.date));
            }
            const auto purchases = reportService.recentPurchases(4);
            for (const auto& item : purchases) {
                recent_->addItem(QString("Purchase %1  •  PKR %2  •  %3").arg(item.invoiceNo).arg(pos::formatPaisa(item.total)).arg(item.date));
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