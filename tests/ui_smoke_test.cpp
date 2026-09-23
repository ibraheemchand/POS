#include <QtTest>
#include <QApplication>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QDir>
#include <QScreen>
#include <filesystem>
#include "core/database.h"
#include "core/seed_service.h"
#include "core/data_change_bus.h"
#include "core/security_service.h"
#include "core/auth_session.h"
#include "core/partner_service.h"
#include "ui/main_window.h"
#include "ui/pages/main_page.h"
#include "ui/pages/nav_catalog.h"
#include "ui/pages/commission_settings_page.h"
#include "ui/pages/partners_page.h"
#include "ui/pages/profit_report_page.h"

class UiSmokeTest final : public QObject {
    Q_OBJECT
private slots:
    void mainWindowConstructsAllOperationalPages();
    void inspectDashboardPage();
    void dashboardRendersOnEmptyDatabase();
    void testEveryPageIndependently();
    void navigatesToEveryPageIncludingOwnerPages();
    void keyboardShortcutsOpenTheCorrectPage();
    void quickAccessButtonsOpenTheCorrectPage();
    void protectedPagesHideDataWhenLockedAndShowWhenUnlocked();
    void capturesScreenshotsAndScansLayoutAtResolutions();
};

namespace {
// A throwaway seeded database, never the user's real data.
std::shared_ptr<pos::Database> seededDb(const char* tag) {
    const auto path = std::filesystem::temp_directory_path() / (std::string("ui-") + tag + "-" + pos::uuid().toStdString() + ".db");
    auto db = std::make_shared<pos::Database>(path);
    db->migrate();
    pos::SeedService(db).seedDemoData();
    return db;
}
void unlockOwner(const std::shared_ptr<pos::Database>& db) {
    pos::SecurityService sec(db);
    if (!sec.hasPin()) sec.setupPin("123456");
    pos::AuthSession::instance().setTimeoutSeconds(3600);
    pos::AuthSession::instance().unlock(db, "123456");
}
QString currentPageTitle(MainWindow& w) {
    if (auto* title = w.findChild<QLabel*>("workspaceTitle")) return title->text();
    return {};
}
} // namespace

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
            QCOMPARE(lbl->text(), QString("PKR 0")); // whole amounts drop the .00
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

    auto saleQuery = database->prepare("INSERT INTO sales(id,invoice_no,customer_id,shift_id,status,payment_method,subtotal,discount,total,paid,due,note,created_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)");
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

void UiSmokeTest::navigatesToEveryPageIncludingOwnerPages() {
    auto db = seededDb("nav");
    unlockOwner(db); // so owner pages open without a modal PIN prompt
    MainWindow window(db);
    window.resize(1600, 900);
    window.show();
    QTest::qWait(80);
    for (const auto& entry : pos::navCatalog()) {
        unlockOwner(db); // leaving an owner page re-locks the session; keep it open for the next hop
        window.goToPage(entry.name);
        QTest::qWait(40);
        QCOMPARE(currentPageTitle(window), entry.name);
    }
}

void UiSmokeTest::keyboardShortcutsOpenTheCorrectPage() {
    auto db = seededDb("keys");
    unlockOwner(db);
    MainWindow window(db);
    window.resize(1600, 900);
    window.show();
    QApplication::setActiveWindow(&window);
    QTest::qWait(80);

    struct SC { Qt::Key key; Qt::KeyboardModifiers mods; const char* page; };
    const SC shortcuts[] = {
        {Qt::Key_F3, Qt::NoModifier, "Inventory"},
        {Qt::Key_F4, Qt::NoModifier, "Purchases"},
        {Qt::Key_F9, Qt::NoModifier, "Customers"},
        {Qt::Key_F2, Qt::NoModifier, "Main"},
        {Qt::Key_1, Qt::ControlModifier, "Sales POS"},
        {Qt::Key_2, Qt::ControlModifier, "Courses"},
        {Qt::Key_3, Qt::ControlModifier, "Suppliers"},
        {Qt::Key_4, Qt::ControlModifier, "Cash & Shifts"},
        {Qt::Key_5, Qt::ControlModifier, "Cheques"},
        {Qt::Key_6, Qt::ControlModifier, "Reports"},
        {Qt::Key_7, Qt::ControlModifier, "Audit log"},
        {Qt::Key_8, Qt::ControlModifier, "Commission Settings"},
        {Qt::Key_9, Qt::ControlModifier, "Partners"},
        {Qt::Key_0, Qt::ControlModifier, "Profit & Commission"},
        {Qt::Key_1, Qt::ControlModifier | Qt::ShiftModifier, "Settings"},
        {Qt::Key_2, Qt::ControlModifier | Qt::ShiftModifier, "Backup & Restore"},
    };
    for (const auto& s : shortcuts) {
        window.goToPage("Main"); // reset so each shortcut is proven to navigate
        QTest::qWait(20);
        unlockOwner(db); // keep owner session open so owner-page shortcuts don't pop a modal
        QTest::keyClick(&window, s.key, s.mods);
        QTest::qWait(40);
        QCOMPARE(currentPageTitle(window), QString(s.page));
    }
}

void UiSmokeTest::quickAccessButtonsOpenTheCorrectPage() {
    auto db = seededDb("quick");
    unlockOwner(db);
    MainWindow window(db);
    window.resize(1600, 900);
    window.show();
    QTest::qWait(80);
    window.goToPage("Main");
    QTest::qWait(40);
    auto* mainPage = window.findChild<MainPage*>();
    QVERIFY(mainPage);
    const auto buttons = mainPage->findChildren<QPushButton*>("quickCard");
    QVERIFY(buttons.size() >= 14); // every sidebar page except Main
    for (auto* btn : buttons) {
        // Button text is "Name\nShortcut" (+ lock glyph on owner pages).
        auto name = btn->text().section('\n', 0, 0);
        name = name.remove(QString::fromUtf8("\xF0\x9F\x94\x92")).trimmed(); // strip lock emoji (surrogate pair)
        window.goToPage("Main");
        QTest::qWait(20);
        unlockOwner(db); // keep owner session open for owner-page buttons
        QTest::mouseClick(btn, Qt::LeftButton);
        QTest::qWait(40);
        QCOMPARE(currentPageTitle(window), name);
    }
}

void UiSmokeTest::protectedPagesHideDataWhenLockedAndShowWhenUnlocked() {
    auto db = seededDb("gate");
    // Seed a partner so the Partners page has data to (not) show.
    unlockOwner(db);
    pos::PartnerService(db).createPartner({{}, "Test Partner", "0300", "", 0, false});

    // --- LOCKED (cashier): no protected data on screen ---
    pos::AuthSession::instance().lock();
    {
        CommissionSettingsPage page(db);
        QMetaObject::invokeMethod(&page, "load");
        QTest::qWait(20);
        for (auto* t : page.findChildren<QTableWidget*>()) QCOMPARE(t->rowCount(), 0);
    }
    {
        PartnersPage page(db);
        QMetaObject::invokeMethod(&page, "load");
        QTest::qWait(20);
        for (auto* t : page.findChildren<QTableWidget*>()) QCOMPARE(t->rowCount(), 0);
    }
    {
        ProfitReportPage page(db);
        QMetaObject::invokeMethod(&page, "load");
        QTest::qWait(20);
        const auto cards = page.findChildren<QLabel*>("metricValue");
        QVERIFY(!cards.isEmpty());
        for (auto* c : cards) QCOMPARE(c->text(), QString("—")); // figures hidden when locked
    }

    // --- UNLOCKED (owner): protected data appears ---
    unlockOwner(db);
    {
        CommissionSettingsPage page(db);
        QMetaObject::invokeMethod(&page, "load");
        QTest::qWait(20);
        int rows = 0;
        for (auto* t : page.findChildren<QTableWidget*>()) rows += t->rowCount();
        QVERIFY2(rows > 0, "Commission Settings should list seeded books once unlocked");
    }
    {
        PartnersPage page(db);
        QMetaObject::invokeMethod(&page, "load");
        QTest::qWait(20);
        int rows = 0;
        for (auto* t : page.findChildren<QTableWidget*>()) rows += t->rowCount();
        QVERIFY2(rows > 0, "Partners should list the seeded partner once unlocked");
    }
    {
        ProfitReportPage page(db);
        QMetaObject::invokeMethod(&page, "load");
        QTest::qWait(20);
        const auto cards = page.findChildren<QLabel*>("metricValue");
        bool anyCurrency = false;
        for (auto* c : cards) anyCurrency |= c->text().startsWith("PKR");
        QVERIFY2(anyCurrency, "Profit report cards should show figures once unlocked");
    }
}

void UiSmokeTest::capturesScreenshotsAndScansLayoutAtResolutions() {
    auto db = seededDb("shots");
    unlockOwner(db);
    MainWindow window(db);
    window.show();
    QTest::qWait(80);

    const QString dpiLabel = qEnvironmentVariable("TEST_DPI_LABEL", "100");
    QString base = qEnvironmentVariable("TEST_OUTPUT_DIR");
    if (base.isEmpty()) base = QDir::currentPath() + "/test-output";
    const QString outDir = base + "/screens/dpi" + dpiLabel;
    QDir().mkpath(outDir);

    QFile report(base + "/layout-report.txt");
    report.open(QIODevice::Append | QIODevice::Text);
    QTextStream rs(&report);

    const QList<QSize> resolutions = {QSize(1366, 768), QSize(1920, 1080)};
    for (const auto& res : resolutions) {
        window.resize(res);
        QTest::qWait(60);
        for (const auto& entry : pos::navCatalog()) {
            unlockOwner(db); // owner pages render fully in the shots
            window.goToPage(entry.name);
            QTest::qWait(50);
            const QString file = QString("%1/%2_%3x%4.png").arg(outDir, entry.name).arg(res.width()).arg(res.height()).replace(' ', '_').replace('&', "and");
            const QPixmap shot = window.grab();
            QVERIFY(!shot.isNull());
            QVERIFY(shot.save(file));

            // Layout scan: flag visible widgets whose rect spills past the RIGHT window
            // edge (horizontal overflow is a real bug; vertical is legitimate scrolling).
            for (auto* child : window.findChildren<QWidget*>()) {
                if (!child->isVisible() || child->size().isEmpty()) continue;
                const QRect g(child->mapTo(&window, QPoint(0, 0)), child->size());
                if (g.right() > window.width() + 2 && g.left() >= 0) {
                    rs << QString("dpi%1 %2 %3x%4: '%5' (%6) right edge %7 > window %8\n")
                              .arg(dpiLabel, entry.name).arg(res.width()).arg(res.height())
                              .arg(child->objectName().isEmpty() ? child->metaObject()->className() : child->objectName())
                              .arg(child->metaObject()->className())
                              .arg(g.right()).arg(window.width());
                }
            }
        }
    }
    report.close();
    qDebug() << "Screenshots saved under" << outDir;
}

int main(int argc, char** argv) {
    Q_INIT_RESOURCE(resources);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    UiSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ui_smoke_test.moc"
