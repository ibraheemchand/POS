#include "ui/pages/dashboard_page.h"
#include "ui/pages/sales_trend_graph.h"
#include "ui/pages/page_helper.h"
#include "core/database.h"
#include "core/report_service.h"
#include "core/inventory_service.h"
#include "core/customer_service.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QPushButton>
#include <QTime>
#include <QDate>

DashboardPage::DashboardPage(std::shared_ptr<pos::Database> database, QWidget* parent)
    : QWidget(parent), database_(std::move(database)) {
    auto* l = new QVBoxLayout(this);
    l->setContentsMargins(0, 12, 0, 0);
    l->setSpacing(14);

    auto* heading = new QHBoxLayout;
    auto* title = new QLabel(this);
    title->setObjectName("pageTitle");
    const auto hour = QTime::currentTime().hour();
    title->setText(hour < 12 ? "Good morning" : hour < 17 ? "Good afternoon" : "Good evening");

    auto* intro = new QLabel("Here’s a live view of your wholesale operation.", this);
    intro->setObjectName("muted");
    auto* tx = new QVBoxLayout;
    tx->addWidget(title);
    tx->addWidget(intro);
    heading->addLayout(tx);
    heading->addStretch();

    auto* backup = new QPushButton("Backup now", this);
    backup->setObjectName("primary");
    connect(backup, &QPushButton::clicked, this, [this] {
        emit requestNavigation("Backup & Restore");
    });
    heading->addWidget(backup);
    l->addLayout(heading);

    metricValues_.resize(6);
    metricCaptions_.resize(6);
    auto* metrics = new QGridLayout;
    metrics->setHorizontalSpacing(14);
    metrics->setVerticalSpacing(14);

    const QList<QString> accents = {"#2563EB", "#22C55E", "#F59E0B", "#EF4444", "#7C3AED", "#EA580C"};
    const QList<QString> labels = {"Today's sales", "Today's cash", "Receivables", "Low stock", "Today's purchases", "Expiring batches"};

    for (int i = 0; i < 6; ++i) {
        auto* card = new QFrame(this);
        card->setObjectName("metric");
        card->setMinimumHeight(108);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 15, 16, 15);
        cardLayout->setSpacing(6);

        auto* label = new QLabel(labels[i], card);
        label->setObjectName("metricLabel");

        auto* value = new QLabel("—", card);
        value->setObjectName("metricValue");
        value->setStyleSheet("color:" + accents[i] + ";");

        auto* caption = new QLabel("", card);
        caption->setObjectName("metricCaption");

        cardLayout->addWidget(label);
        cardLayout->addWidget(value);
        cardLayout->addWidget(caption);

        metricValues_[i] = value;
        metricCaptions_[i] = caption;
        metrics->addWidget(card, i / 3, i % 3);
    }
    l->addLayout(metrics);

    auto* middle = new QHBoxLayout;
    middle->setSpacing(16);

    auto* quickPanel = new QFrame(this);
    quickPanel->setObjectName("panel");
    auto* ql = new QVBoxLayout(quickPanel);
    ql->setContentsMargins(20, 18, 20, 18);
    ql->setSpacing(12);

    auto* qt = new QLabel("Quick actions", quickPanel);
    qt->setObjectName("sectionTitle");
    auto* qhint = new QLabel("Jump straight to the task you need.", quickPanel);
    qhint->setObjectName("muted");
    ql->addWidget(qt);
    ql->addWidget(qhint);

    const QStringList actions = {
        "New sale", "New purchase", "Add product", "Add customer", 
        "Add supplier", "Receive payment", "Cash entry", "Current stock", 
        "Sales register", "Purchase register", "Receivables due", "Reports"
    };
    const QStringList targets = {
        "Sales POS", "Purchases", "Inventory", "Customers", 
        "Suppliers", "Customers", "Cash & Shifts", "Inventory", 
        "Reports", "Reports", "Customers", "Reports"
    };

    auto* grid = new QGridLayout;
    grid->setSpacing(10);
    for (int i = 0; i < actions.size(); ++i) {
        auto* b = new QPushButton(actions[i], quickPanel);
        b->setObjectName("quick");
        const auto target = targets[i];
        connect(b, &QPushButton::clicked, this, [this, target] {
            emit requestNavigation(target);
        });
        grid->addWidget(b, i / 4, i % 4);
    }
    ql->addLayout(grid);
    middle->addWidget(quickPanel, 3);

    auto* graphPanel = new QFrame(this);
    graphPanel->setObjectName("panel");
    auto* gl = new QVBoxLayout(graphPanel);
    gl->setContentsMargins(12, 12, 12, 12);

    chart_ = new SalesTrendGraph(graphPanel);
    gl->addWidget(chart_);
    middle->addWidget(graphPanel, 2);
    l->addLayout(middle);

    auto* lower = new QHBoxLayout;
    lower->setSpacing(16);

    auto* activity = new QFrame(this);
    activity->setObjectName("panel");
    auto* al = new QVBoxLayout(activity);
    al->setContentsMargins(20, 18, 20, 18);
    al->setSpacing(10);
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
    auto* nl = new QVBoxLayout(lowStockAlerts);
    nl->setContentsMargins(20, 18, 20, 18);
    nl->setSpacing(10);
    auto* nt = new QLabel("Low Stock Alerts", lowStockAlerts);
    nt->setObjectName("sectionTitle");
    nl->addWidget(nt);

    lowStockList_ = new QListWidget(lowStockAlerts);
    lowStockList_->setObjectName("recentList");
    lowStockList_->setFocusPolicy(Qt::NoFocus);
    nl->addWidget(lowStockList_, 1);
    lower->addWidget(lowStockAlerts, 2);

    l->addLayout(lower, 1);

    // Set tab order
    setTabOrder(backup, quickPanel);
}

void DashboardPage::load() {
    try {
        const auto today = QDate::currentDate();
        pos::ReportService reportService(database_);
        pos::InventoryService inventoryService(database_);
        pos::CustomerService customerService(database_);

        // 1. Today's sales
        const auto salesVal = reportService.summary(today, today).sales;
        metricValues_[0]->setText("PKR " + pos::formatPaisa(salesVal));
        const auto count = reportService.salesCount(today);
        metricCaptions_[0]->setText(QString("%1 completed invoices").arg(count));

        // 2. Today's cash
        const auto cashVal = reportService.todayCashSales(today);
        metricValues_[1]->setText("PKR " + pos::formatPaisa(cashVal));
        metricCaptions_[1]->setText("Cash payments received");

        // 3. Receivables
        const auto recVal = customerService.totalReceivables();
        metricValues_[2]->setText("PKR " + pos::formatPaisa(recVal));
        metricCaptions_[2]->setText("Unpaid customer balances");

        // 4. Low stock
        const auto lowStockAlertsList = inventoryService.lowStock();
        metricValues_[3]->setText(QString::number(lowStockAlertsList.size()));
        metricCaptions_[3]->setText("Products below threshold");

        // 5. Today's purchases
        const auto purVal = reportService.summary(today, today).purchases;
        metricValues_[4]->setText("PKR " + pos::formatPaisa(purVal));
        metricCaptions_[4]->setText("Received stock value");

        // 6. Expiring batches
        const auto expiringVal = reportService.expiringBatchesCount(today, today.addDays(30));
        metricValues_[5]->setText(QString::number(expiringVal));
        metricCaptions_[5]->setText("Batches expiring within 30 days");

        // 7. Trend chart
        QVector<double> trendValues;
        QStringList trendLabels;
        const auto trend = reportService.salesTrend(today.addDays(-6), today);
        for (const auto& pair : trend) {
            trendValues.append(static_cast<double>(pair.second) / 100.0);
            trendLabels.append(QDate::fromString(pair.first, Qt::ISODate).toString("dd MMM"));
        }
        chart_->setData(trendValues, trendLabels);

        // 8. Recent Activity
        recent_->clear();
        const auto sales = reportService.recentSales(4);
        for (const auto& item : sales) {
            recent_->addItem(QString("Sale %1  •  PKR %2  •  %3").arg(item.invoiceNo).arg(pos::formatPaisa(item.total)).arg(item.date));
        }
        const auto purchases = reportService.recentPurchases(4);
        for (const auto& item : purchases) {
            recent_->addItem(QString("Purchase %1  •  PKR %2  •  %3").arg(item.invoiceNo).arg(pos::formatPaisa(item.total)).arg(item.date));
        }

        // 9. Low Stock list
        lowStockList_->clear();
        const auto lowStockItems = inventoryService.listLowStock(10);
        for (const auto& item : lowStockItems) {
            lowStockList_->addItem(QString("%1 — %2 / %3 %4").arg(item.name).arg(item.quantity).arg(item.minimumStock).arg(item.baseUnit));
        }
    } catch (...) {}
}
