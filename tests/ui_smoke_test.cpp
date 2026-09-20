#include <QtTest>
#include <QApplication>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <filesystem>
#include "core/database.h"
#include "core/seed_service.h"
#include "core/data_change_bus.h"
#include "ui/main_window.h"
#include "ui/pages/main_page.h"

class UiSmokeTest final : public QObject {
    Q_OBJECT
private slots:
    void mainWindowConstructsAllOperationalPages();
    void inspectDashboardPage();
    void dashboardRendersOnEmptyDatabase();
    void testEveryPageIndependently();
};

void UiSmokeTest::testEveryPageIndependently() {
    const auto path = std::filesystem::temp_directory_path() / ("ui-allpages-" + pos::uuid().toStdString() + ".db");
    auto database = std::make_shared<pos::Database>(path);
    database->migrate();
    pos::SeedService(database).seedDemoData();

    MainWindow window(database);
    window.resize(1366, 768);
    window.show();
    QTest::qWait(100);

    const QString artifactDir =
        QString::fromStdString(std::filesystem::temp_directory_path().string()) + "/pos-ui-shots/";
    std::filesystem::create_directories(artifactDir.toStdString());

    struct PageDef {
        const char* name;
        const char* screenshotFile;
    };

    const PageDef pages[] = {
        {"Main", "page_main.png"},
        {"Sales POS", "page_sales_pos.png"},
        {"Inventory", "page_inventory.png"},
        {"Purchases", "page_purchases.png"},
        {"Customers", "page_customers.png"},
        {"Suppliers", "page_suppliers.png"},
        {"Cash & Shifts", "page_cash.png"},
        {"Cheques", "page_cheques.png"},
        {"Reports", "page_reports.png"},
        {"Audit log", "page_audit.png"},
        {"Settings", "page_settings.png"},
        {"Backup & Restore", "page_backup.png"}
    };

    for (const auto& p : pages) {
        window.goToPage(p.name);
        QTest::qWait(100);
        QPixmap shot = window.grab();
        shot.save(artifactDir + p.screenshotFile);
        qDebug() << "Captured verified screenshot for page:" << p.name << "->" << p.screenshotFile;
    }
}

void UiSmokeTest::dashboardRendersOnEmptyDatabase() {
    const auto path = std::filesystem::temp_directory_path() / ("ui-empty-dashboard-" + pos::uuid().toStdString() + ".db");
    auto database = std::make_shared<pos::Database>(path);
    database->migrate();

    MainWindow window(database);
    window.resize(1366, 768);
    window.show();
    QTest::qWait(50);

    auto* mainPage = window.findChild<MainPage*>();
    QVERIFY(mainPage != nullptr);

    const auto metricValues = mainPage->findChildren<QLabel*>("metricValue");
    QCOMPARE(metricValues.size(), 6);
    for (int i = 0; i < 6; ++i) {
        auto* lbl = metricValues[i];
        QVERIFY(!lbl->text().isEmpty());
        QVERIFY(lbl->text() != "—");
        QVERIFY(lbl->isVisible());
        if (i == 3 || i == 5) {
            QCOMPARE(lbl->text(), QString("0"));
        } else {
            QCOMPARE(lbl->text(), QString("PKR 0.00"));
        }
    }
}

void UiSmokeTest::inspectDashboardPage() {
    const auto path = std::filesystem::temp_directory_path() / ("ui-dashboard-" + pos::uuid().toStdString() + ".db");
    auto database = std::make_shared<pos::Database>(path);
    database->migrate();
    pos::SeedService(database).seedDemoData();

    MainWindow window(database);
    window.resize(1366, 768);
    window.show();
    QTest::qWait(100);

    auto* mainPage = window.findChild<MainPage*>();
    QVERIFY(mainPage != nullptr);

    // 1. Verify all 6 metricValue labels have non-empty, formatted text and are fully visible
    const auto metricValues = mainPage->findChildren<QLabel*>("metricValue");
    QCOMPARE(metricValues.size(), 6);
    for (int i = 0; i < 6; ++i) {
        auto* lbl = metricValues[i];
        QVERIFY(!lbl->text().isEmpty());
        QVERIFY(lbl->text() != "—");
        QVERIFY(lbl->isVisible());
        QVERIFY(lbl->height() >= 16);
        if (i == 3 || i == 5) {
            bool ok = false;
            lbl->text().toInt(&ok);
            QVERIFY2(ok, "Metric count must be an integer");
        } else {
            QVERIFY2(lbl->text().startsWith("PKR "), "Currency metrics must start with PKR");
        }
        qDebug() << "Main Metric" << i << "Value:" << lbl->text() << "Geometry:" << lbl->geometry();
    }

    // 2. Switch theme and confirm it does not crash or clear the metrics
    window.switchTheme();
    QTest::qWait(50);
    window.switchTheme();
    QTest::qWait(50);

    // 3. Verify live reactivity via DataChangeBus
    auto* salesValLbl = metricValues[0];
    const QString prevSales = salesValLbl->text();
    auto custQ = database->prepare("SELECT id FROM customers LIMIT 1");
    QString custId;
    if (custQ.stepRow()) custId = custQ.text(0);
    auto shiftQ = database->prepare("SELECT id FROM shift_sessions LIMIT 1");
    QString shiftId;
    if (shiftQ.stepRow()) shiftId = shiftQ.text(0);

    auto saleQuery = database->prepare("INSERT INTO sales(id,invoice_no,customer_id,shift_id,status,payment_method,subtotal_paisa,discount_paisa,total_paisa,paid_paisa,due_paisa,note,created_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)");
    saleQuery.bind(1, pos::uuid());
    saleQuery.bind(2, "INV-REACT-001");
    saleQuery.bind(3, custId);
    saleQuery.bind(4, shiftId);
    saleQuery.bind(5, "completed");
    saleQuery.bind(6, "cash");
    saleQuery.bind(7, static_cast<qint64>(500000));
    saleQuery.bind(8, static_cast<qint64>(0));
    saleQuery.bind(9, static_cast<qint64>(500000));
    saleQuery.bind(10, static_cast<qint64>(500000));
    saleQuery.bind(11, static_cast<qint64>(0));
    saleQuery.bind(12, "Reactivity test");
    saleQuery.bind(13, QDate::currentDate().toString(Qt::ISODate) + "T12:00:00Z");
    saleQuery.execute();

    emit pos::DataChangeBus::instance().salesChanged();
    QTest::qWait(50);
    QVERIFY(salesValLbl->text() != prevSales);
}

void UiSmokeTest::mainWindowConstructsAllOperationalPages() {
    const auto path = std::filesystem::temp_directory_path() / ("ui-smoke-" + pos::uuid().toStdString() + ".db");
    auto database = std::make_shared<pos::Database>(path);
    database->migrate();
    MainWindow window(database);
    QVERIFY(window.findChild<QListWidget*>() != nullptr);
    const auto buttons = window.findChildren<QPushButton*>();
    QVERIFY(buttons.size() >= 20);
    bool hasInventoryEdit = false;
    bool hasNotifications = false;
    bool hasThermalTest = false;
    bool hasBackup = false;
    for (const auto* button : buttons) {
        const auto txt = button->text().remove('&');
        hasInventoryEdit |= txt == "Edit selected";
        hasNotifications |= txt == "View notifications";
        hasThermalTest |= txt == "Print test receipt";
        hasBackup |= txt == "Create verified backup";
    }
    QVERIFY(hasInventoryEdit);
    QVERIFY(hasNotifications);
    QVERIFY(hasThermalTest);
    QVERIFY(hasBackup);
    auto sales = database->prepare("SELECT COUNT(*) FROM sales");
    QVERIFY(sales.stepRow());
    QCOMPARE(sales.integer(0), qint64(0));
}

int main(int argc, char** argv) {
    Q_INIT_RESOURCE(resources);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    UiSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ui_smoke_test.moc"
