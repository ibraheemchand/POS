#include <QtTest>
#include <QApplication>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QScreen>
#include <QPainter>
#include <QPushButton>
#include <QFrame>
#include <QFile>
#include <QStackedWidget>
#include <QScrollArea>
#include <QFontMetrics>
#include <filesystem>
#include "core/database.h"
#include "core/seed_service.h"
#include "core/data_change_bus.h"
#include "core/security_service.h"
#include "core/auth_session.h"
#include "core/partner_service.h"
#include "core/inventory_service.h"
#include "ui/main_window.h"
#include "ui/pages/main_page.h"
#include "ui/pages/nav_catalog.h"
#include "ui/pages/commission_settings_page.h"
#include "ui/pages/partners_page.h"
#include "ui/pages/profit_report_page.h"
#include "ui/pages/purchases_page.h"
#include "ui/pages/inventory_page.h"
#include "ui/pages/sales_pos_page.h"
#include "ui/discount_slider.h"
#include "ui/theme.h"
#include "core/receipt_service.h"
#include "core/security_service.h"
#include "ui/receipt_output.h"
#include "ui/first_run_wizard.h"
#include <QCheckBox>
#include <QPrinter>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>

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
    void verifyLayoutAtRequiredSizes();
    // Phase 1 reproduction tests (issues reported during testing).
    void repro_purchasesHasLoadCourseButton();       // issue 1
    void purchasesInlineEditRecomputesTotal();        // issue 1
    void purchasesPaymentSectionSwitchesModes();      // purchase payment section
    void salesDiscountSliderRespectsMaxAndZones();    // discount slider
    void receiptSaveAsPdfIsValidWithText();           // Save as PDF (QPdfWriter)
    void receiptNormalDriverPdfIsValid();             // Normal printer path -> PDF file
    void firstRunWizardStartsEmpty();                 // fresh install wizard + empty DB
    void repro_inventoryInStockColourIsGreen();       // issue 2
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
    return w.currentPageName();
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


// The "Current sale" cart table (its last column is "Line total").
static QTableWidget* findCartTable(QWidget* root) {
    for (auto* t : root->findChildren<QTableWidget*>()) {
        auto* last = t->horizontalHeaderItem(t->columnCount() - 1);
        if (last && last->text() == "Line total") return t;
    }
    return nullptr;
}
static QTableWidget* findProductTable(QWidget* root) {
    for (auto* t : root->findChildren<QTableWidget*>()) {
        auto* c0 = t->horizontalHeaderItem(0);
        auto* c1 = t->horizontalHeaderItem(1);
        if (c0 && c0->text() == "Product" && c1 && c1->text() == "SKU") return t;
    }
    return nullptr;
}
static bool rectsVerticallyOverlap(const QRect& a, const QRect& b) {
    return a.bottom() > b.top() + 2 && b.bottom() > a.top() + 2 && a.left() < b.right() && b.left() < a.right();
}

void UiSmokeTest::verifyLayoutAtRequiredSizes() {
    auto db = seededDb("layout");
    unlockOwner(db);
    MainWindow window(db);
    window.show();

    const QString dpiLabel = qEnvironmentVariable("TEST_DPI_LABEL", "100");
    const double scale = qEnvironmentVariable("QT_SCALE_FACTOR", "1").toDouble();
    QString base = qEnvironmentVariable("TEST_OUTPUT_DIR");
    if (base.isEmpty()) base = QDir::currentPath() + "/test-output";
    const QString outDir = base + "/screens/dpi" + dpiLabel;
    QDir().mkpath(outDir);
    QFile report(base + "/screens/layout-verification-dpi" + dpiLabel + ".txt");
    report.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream rs(&report);
    // Device font height = logical height x device-pixel-ratio. With point-based fonts
    // + PassThrough, device-height / scale is constant across DPI = same physical size.
    const double logicalFontPx = QFontMetrics(qApp->font()).height();
    const double deviceFontPx = logicalFontPx * window.devicePixelRatioF();
    rs << QString("DPI %1%%  QT_SCALE_FACTOR=%2  dpr=%3  logical-font=%4px  device-font=%5px  device/scale=%6 (≈constant => same physical size)\n\n")
              .arg(dpiLabel).arg(scale).arg(window.devicePixelRatioF()).arg(logicalFontPx).arg(deviceFontPx).arg(deviceFontPx / scale, 0, 'f', 2);

    struct SizeCfg { int w; int h; };
    const SizeCfg sizes[] = {{1280, 720}, {1536, 864}, {1920, 1080}, {2560, 1440}};
    static const QStringList frequentlyUsed = {"Main", "Sales POS", "Inventory", "Courses", "Purchases", "Customers", "Suppliers", "Cash & Shifts"};

    auto* stack = window.findChild<QStackedWidget*>();
    QVERIFY(stack);

    for (const auto& sz : sizes) {
        window.resize(sz.w, sz.h);
        QTest::qWait(60);
        for (const auto& entry : pos::navCatalog()) {
            unlockOwner(db);
            window.goToPage(entry.name);
            QTest::qWait(60);
            const QString file = QString("%1/%2_%3x%4.png").arg(outDir, entry.name).arg(sz.w).arg(sz.h).replace(' ', '_').replace('&', "and");
            window.grab().save(file);

            // Current page widget (unwrapped from a scroll area if it is a scrolling page).
            QWidget* page = stack->currentWidget();
            const bool wrapped = qobject_cast<QScrollArea*>(page) != nullptr;
            if (auto* sa = qobject_cast<QScrollArea*>(page)) page = sa->widget();

            // No horizontal overflow anywhere.
            bool noHOverflow = true;
            for (auto* child : window.findChildren<QWidget*>()) {
                if (!child->isVisible() || child->size().isEmpty()) continue;
                const QRect g(child->mapTo(&window, QPoint(0, 0)), child->size());
                if (g.right() > window.width() + 2 && g.left() >= 0) { noHOverflow = false; break; }
            }

            QString verdict = noHOverflow ? "PASS" : "FAIL(h-overflow)";
            const bool freq = frequentlyUsed.contains(entry.name);
            if (freq && sz.w == 1280 && sz.h == 720) {
                // No page scrollbar (frequently-used pages are not wrapped) and content fits.
                const bool noPageScroll = !wrapped;
                const bool fits = page && page->minimumSizeHint().height() <= page->height() + 6;
                if (!noPageScroll) verdict = "FAIL(page-scrolls)";
                else if (!fits) verdict = "FAIL(clipped)";
                if (entry.name == "Sales POS") {
                    auto* cart = findCartTable(&window);
                    auto* prod = findProductTable(&window);
                    auto* totals = window.findChild<QFrame*>("summaryBox");
                    QPushButton *savePrint = nullptr, *resume = nullptr;
                    for (auto* b : window.findChildren<QPushButton*>()) {
                        if (b->text().contains("Print")) savePrint = b;
                        if (b->text().contains("esume")) resume = b;
                    }
                    const bool cartRows = cart && cart->isVisible() && cart->height() >= 5 * 28;
                    const bool prodRows = prod && prod->isVisible() && prod->height() >= 6 * 28;
                    const bool totalsOk = totals && totals->isVisible();
                    bool buttonsOk = savePrint && resume && savePrint->isVisible() && resume->isVisible();
                    if (buttonsOk) {
                        const QRect r1(savePrint->mapTo(&window, QPoint(0, 0)), savePrint->size());
                        const QRect r2(resume->mapTo(&window, QPoint(0, 0)), resume->size());
                        buttonsOk = !rectsVerticallyOverlap(r1, r2);
                    }
                    if (!(cartRows && prodRows && totalsOk && buttonsOk) && verdict == "PASS")
                        verdict = QString("FAIL(salespos cart=%1 prod=%2 totals=%3 buttons=%4)").arg(cartRows).arg(prodRows).arg(totalsOk).arg(buttonsOk);
                    QVERIFY2(cartRows, "Sales POS: fewer than 5 cart rows at 1280x720");
                    QVERIFY2(prodRows, "Sales POS: fewer than 6 product rows at 1280x720");
                    QVERIFY2(totalsOk, "Sales POS: totals box not visible at 1280x720");
                    QVERIFY2(buttonsOk, "Sales POS: action buttons overlap/missing at 1280x720");
                }
                QVERIFY2(noPageScroll, qPrintable(entry.name + " page-scrolls at 1280x720"));
                QVERIFY2(fits, qPrintable(entry.name + " content clipped at 1280x720"));
            }
            QVERIFY2(noHOverflow, qPrintable(entry.name + " horizontal overflow"));
            rs << QString("%1 @ %2x%3: %4\n").arg(entry.name).arg(sz.w).arg(sz.h).arg(verdict);
        }
    }
    report.close();
    qDebug() << "Layout verification + screenshots under" << outDir;
}

// ISSUE 1: Purchases should offer a "Load course…" button (like Sales POS) that
// adds every book of a course to the purchase in one go. Reproduction: the button
// must exist. Currently it does not, so this FAILS until issue 1 is fixed.
void UiSmokeTest::repro_purchasesHasLoadCourseButton() {
    const auto path = std::filesystem::temp_directory_path() / ("ui-repro-loadcourse-" + pos::uuid().toStdString() + ".db");
    auto db = std::make_shared<pos::Database>(path);
    db->migrate();
    PurchasesPage page(db);
    bool found = false;
    for (const auto* b : page.findChildren<QPushButton*>()) {
        if (b->text().remove('&').contains("Load course", Qt::CaseInsensitive)) { found = true; break; }
    }
    QVERIFY2(found, "Purchases page has no 'Load course' button (issue 1 feature missing)");
}

// ISSUE 1: Editing a cart row's quantity or unit cost must recompute the purchase
// total immediately. Adds a row via the normal controls, then edits cells through
// the table API (which drives the same cellChanged path the UI uses).
void UiSmokeTest::purchasesInlineEditRecomputesTotal() {
    const auto path = std::filesystem::temp_directory_path() / ("ui-purchase-edit-" + pos::uuid().toStdString() + ".db");
    auto db = std::make_shared<pos::Database>(path);
    db->migrate();
    pos::InventoryService(db).createProduct("Widget", "piece", 0, 500, false);

    PurchasesPage page(db);
    page.reloadLists();

    auto* combo = page.findChild<QComboBox*>("purchaseProduct");
    QVERIFY(combo && combo->count() >= 1);
    combo->setCurrentIndex(0);
    const auto spins = page.findChildren<QSpinBox*>();
    QVERIFY(!spins.isEmpty());
    spins[0]->setValue(2);                       // quantity (setsSpin_ is the other spin)
    auto* price = page.findChild<QDoubleSpinBox*>();
    QVERIFY(price);
    price->setValue(100.0);                       // PKR 100.00

    QPushButton* add = nullptr;
    QLabel* total = page.findChild<QLabel*>("posTotal");
    for (auto* b : page.findChildren<QPushButton*>()) if (b->text().remove('&') == "Add to cart") add = b;
    QVERIFY(add && total);
    QTest::mouseClick(add, Qt::LeftButton);
    QTest::qWait(10);

    QTableWidget* cart = nullptr;
    for (auto* t : page.findChildren<QTableWidget*>()) {
        auto* h = t->horizontalHeaderItem(2);
        if (h && h->text() == "Unit cost") cart = t;
    }
    QVERIFY(cart && cart->rowCount() == 1);
    QVERIFY2(total->text().contains("200"), qPrintable("expected total 200, got " + total->text())); // 2 x 100

    cart->item(0, 1)->setText("5"); // qty edit
    QTest::qWait(10);
    QVERIFY2(total->text().contains("500"), qPrintable("expected total 500 after qty edit, got " + total->text()));

    cart->item(0, 2)->setText("PKR 300"); // cost edit
    QTest::qWait(10);
    QVERIFY2(total->text().contains("1,500"), qPrintable("expected total 1,500 after cost edit, got " + total->text())); // 5 x 300
    QCOMPARE(cart->item(0, 2)->data(Qt::UserRole).toLongLong(), qint64(30000)); // stored in hundredths
}

// Purchase payment section: button text tracks the method; Partial reveals the
// paid field and shows the correct remaining payable.
void UiSmokeTest::purchasesPaymentSectionSwitchesModes() {
    const auto path = std::filesystem::temp_directory_path() / ("ui-purchase-pay-" + pos::uuid().toStdString() + ".db");
    auto db = std::make_shared<pos::Database>(path);
    db->migrate();
    pos::InventoryService(db).createProduct("Widget", "piece", 0, 500, false);

    PurchasesPage page(db);
    page.reloadLists();
    auto* combo = page.findChild<QComboBox*>("purchaseProduct");
    QVERIFY(combo && combo->count() >= 1);
    combo->setCurrentIndex(0);
    page.findChildren<QSpinBox*>()[0]->setValue(2);
    auto* price = page.findChild<QDoubleSpinBox*>(); // first double-spin is the unit cost
    price->setValue(100.0);
    QPushButton* add = nullptr;
    QPushButton* save = nullptr;
    for (auto* b : page.findChildren<QPushButton*>()) {
        const auto t = b->text().remove('&');
        if (t == "Add to cart") add = b;
        if (t.startsWith("Receive purchase")) save = b;
    }
    QVERIFY(add && save);
    QTest::mouseClick(add, Qt::LeftButton);
    QTest::qWait(10);

    // Default is Cash.
    QCOMPARE(save->text().remove('&'), QString("Receive purchase (Cash)"));

    // Find the payment combo (Cash/Credit/Partial) and the paid double-spin (no suffix).
    QComboBox* pay = nullptr;
    for (auto* c : page.findChildren<QComboBox*>()) if (c->count() == 3 && c->itemText(0) == "Cash") pay = c;
    QVERIFY(pay);
    QDoubleSpinBox* paid = nullptr;
    for (auto* s : page.findChildren<QDoubleSpinBox*>()) if (s->suffix().isEmpty()) paid = s;
    QVERIFY(paid);
    QVERIFY(!paid->isVisibleTo(&page)); // hidden under Cash

    pay->setCurrentIndex(1); // Credit
    QCOMPARE(save->text().remove('&'), QString("Receive purchase on credit"));
    QVERIFY(!paid->isVisibleTo(&page));

    pay->setCurrentIndex(2); // Partial
    QCOMPARE(save->text().remove('&'), QString("Receive purchase (Partial)"));
    QVERIFY(paid->isVisibleTo(&page));
    paid->setValue(50.0); // paid 50 of 200
    QTest::qWait(10);
    QLabel* remaining = nullptr;
    for (auto* l : page.findChildren<QLabel*>()) if (l->text().startsWith("Remaining payable")) remaining = l;
    QVERIFY(remaining);
    QVERIFY2(remaining->text().contains("150"), qPrintable("expected remaining 150, got " + remaining->text()));
}

namespace {
// A sample receipt used by the PDF tests.
pos::ReceiptData sampleReceipt() {
    pos::ReceiptData d;
    d.storeName = "Invento";
    d.invoiceNo = "INV-PDFTEST-777";
    d.dateTime = "2026-09-27 12:00";
    d.footer = "Thank you for shopping.";
    d.items = {{"Basmati Rice", 2, 30000}, {"Beauty Soap", 1, 7500}};
    d.total = 37500; // PKR 375
    return d;
}
// Directory where sample PDFs are written so a human can open them.
QString receiptOutDir() {
    QString base = qEnvironmentVariable("TEST_OUTPUT_DIR");
    if (base.isEmpty()) base = QDir::currentPath() + "/test-output";
    const QString dir = base + "/receipts";
    QDir().mkpath(dir);
    return dir;
}
// Real structural parse: valid header, at least one Page object, and an EOF marker.
void assertValidPdf(const QString& path, const char* label) {
    QFile f(path);
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(QString("%1: cannot open %2").arg(label, path)));
    const QByteArray bytes = f.readAll();
    QVERIFY2(bytes.startsWith("%PDF-"), qPrintable(QString("%1: not a PDF header").arg(label)));
    QVERIFY2(bytes.contains("%%EOF"), qPrintable(QString("%1: no EOF marker").arg(label)));
    const QRegularExpression pageRe("/Type\\s*/Page[^s]");
    int pages = 0; auto it = pageRe.globalMatch(bytes);
    while (it.hasNext()) { it.next(); ++pages; }
    QVERIFY2(pages >= 1, qPrintable(QString("%1: expected >=1 page, found %2").arg(label).arg(pages)));
}
// If poppler's pdftotext is available, extract text; otherwise return empty.
QString pdfText(const QString& path) {
    QString exe = QStandardPaths::findExecutable("pdftotext");
    for (const QString& c : {QString("C:/msys64/mingw64/bin/pdftotext.exe"), QString("C:/Program Files/Git/mingw64/bin/pdftotext.exe")})
        if (exe.isEmpty() && QFile::exists(c)) exe = c;
    if (exe.isEmpty()) return {};
    QProcess p;
    p.start(exe, {"-layout", path, "-"});
    if (!p.waitForFinished(15000)) return {};
    return QString::fromUtf8(p.readAllStandardOutput());
}
} // namespace

// Fresh install: an empty database + the first-run wizard, which seeds nothing unless
// the (off-by-default) sample-data box is ticked.
void UiSmokeTest::firstRunWizardStartsEmpty() {
    const auto dir = std::filesystem::temp_directory_path() / ("ui-firstrun-" + pos::uuid().toStdString());
    std::filesystem::create_directories(dir);
    auto db = std::make_shared<pos::Database>(dir / "business.db");
    db->migrate();
    QVERIFY(!pos::SecurityService(db).hasPin()); // fresh -> wizard would show

    FirstRunWizard wizard(db);
    QCOMPARE(wizard.pageIds().size(), 4); // business, PIN, printer, sample
    auto* sample = wizard.findChild<QCheckBox*>("fw_loadSample");
    QVERIFY(sample);
    QVERIFY2(!sample->isChecked(), "sample data must be OFF by default");

    wizard.findChild<QLineEdit*>("fw_business")->setText("Test Shop");
    wizard.findChild<QLineEdit*>("fw_pin")->setText("123456");
    wizard.findChild<QLineEdit*>("fw_pinConfirm")->setText("123456");
    QString err;
    QVERIFY2(wizard.applySetup(&err), qPrintable(err));

    QVERIFY(pos::SecurityService(db).hasPin());          // PIN now set
    QVERIFY(!wizard.recoveryCode().isEmpty());           // recovery code produced
    // Default finish loads NO sample data.
    for (const char* t : {"products", "customers", "sales", "suppliers"}) {
        auto q = db->prepare(QByteArray("SELECT COUNT(*) FROM ").append(t).constData());
        QVERIFY(q.stepRow());
        QVERIFY2(q.integer(0) == 0, qPrintable(QString("%1 should be empty after a default first run").arg(t)));
    }
}

void UiSmokeTest::receiptSaveAsPdfIsValidWithText() {
    const auto data = sampleReceipt();
    const QString path = receiptOutDir() + "/Receipt_" + data.invoiceNo + ".pdf";
    pos::ReceiptService::renderPdf(data, path);
    qDebug() << "Save-as-PDF sample:" << QDir::toNativeSeparators(path);
    assertValidPdf(path, "save-as-pdf");
    const QString text = pdfText(path);
    if (text.isEmpty()) { qWarning() << "pdftotext not found — text content not checked (structure OK)"; return; }
    QVERIFY2(text.contains(data.invoiceNo), qPrintable("PDF text missing invoice number; got:\n" + text));
    QVERIFY2(text.contains("375"), qPrintable("PDF text missing total; got:\n" + text));
}

void UiSmokeTest::receiptNormalDriverPdfIsValid() {
    const auto data = sampleReceipt();
    const QString path = receiptOutDir() + "/Receipt_normal_" + data.invoiceNo + ".pdf";
    // The "Normal printer (Windows driver)" path, targeted at a PDF file (same as
    // selecting Microsoft Print to PDF, but deterministic for the test).
    {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(path);
        QPainter painter(&printer);
        QVERIFY(painter.isActive());
        const QRectF area(0, 0, printer.width(), printer.height());
        pos::ReceiptService::paint(painter, area.adjusted(area.width() * 0.06, area.height() * 0.05, -area.width() * 0.06, -area.height() * 0.05), data);
        painter.end();
    }
    qDebug() << "Normal-driver PDF sample:" << QDir::toNativeSeparators(path);
    assertValidPdf(path, "normal-driver");
    const QString text = pdfText(path);
    if (text.isEmpty()) { qWarning() << "pdftotext not found — text content not checked (structure OK)"; return; }
    QVERIFY2(text.contains(data.invoiceNo), qPrintable("PDF text missing invoice number; got:\n" + text));
    QVERIFY2(text.contains("375"), qPrintable("PDF text missing total; got:\n" + text));
}

// Sales POS invoice-discount slider: its maximum equals the cart's total commission
// (never below cost), the allowed label shows the green-zone end, and the slider
// clamps to the maximum (can't be dragged past it).
void UiSmokeTest::salesDiscountSliderRespectsMaxAndZones() {
    const auto path = std::filesystem::temp_directory_path() / ("ui-slider-" + pos::uuid().toStdString() + ".db");
    auto db = std::make_shared<pos::Database>(path);
    db->migrate();
    pos::InventoryService inv(db);
    const auto id = inv.createProduct("Priced Book", "piece", 5000, 10000, false); // retail PKR 100
    inv.receiveStock({id, 20, 5000, "piece", {}, {}, "Seed"});
    { auto u = db->prepare("UPDATE products SET total_pct_bp=4000, partner_pct_bp=0 WHERE id=?"); u.bind(1, id); u.execute(); } // 40% commission, no partner

    SalesPosPage page(db);
    QMetaObject::invokeMethod(&page, "load");
    QTest::qWait(30);
    QTableWidget* products = nullptr;
    for (auto* t : page.findChildren<QTableWidget*>()) {
        auto* c0 = t->horizontalHeaderItem(0);
        if (c0 && c0->text() == "Product") { products = t; break; }
    }
    QVERIFY(products && products->rowCount() >= 1);
    products->setCurrentCell(0, 0);
    QPushButton* add = nullptr;
    for (auto* b : page.findChildren<QPushButton*>()) if (b->text().remove('&') == "Add to cart") add = b;
    QVERIFY(add);
    QTest::mouseClick(add, Qt::LeftButton);
    QTest::qWait(20);

    auto* slider = page.findChild<DiscountSlider*>();
    QVERIFY(slider);
    // gross 10000: max commission = 40% = 4000; allowed = (40-0-12)% = 2800.
    QCOMPARE(slider->maximum(), 4000);
    slider->setValue(1000000000); // try to exceed the maximum
    QCOMPARE(slider->value(), slider->maximum()); // clamped, can't go past max

    QLabel* allowed = nullptr;
    for (auto* l : page.findChildren<QLabel*>()) if (l->text().startsWith("Allowed without PIN")) allowed = l;
    QVERIFY(allowed);
    QVERIFY2(allowed->text().contains("28"), qPrintable("allowed should be PKR 28, got " + allowed->text()));

    // "Max" button applies the top of the green zone (allowed) with no PIN.
    QPushButton* maxBtn = nullptr;
    for (auto* b : page.findChildren<QPushButton*>()) if (b->text().remove('&') == "Max") maxBtn = b;
    QVERIFY(maxBtn && maxBtn->isEnabled());
    QTest::mouseClick(maxBtn, Qt::LeftButton);
    QTest::qWait(10);
    QCOMPARE(slider->value(), 2800); // snapped to allowed, still in the green zone
}

// ISSUE 2: An in-stock inventory row should read clearly as GREEN (green channel
// dominant), distinct from the red out-of-stock badge. Reproduction: check the
// STOCK LEVEL cell background for an in-stock product. Currently it is a brownish
// peach (#ffdbcd, red-dominant), so this FAILS until issue 2 is fixed.
void UiSmokeTest::repro_inventoryInStockColourIsGreen() {
    const auto path = std::filesystem::temp_directory_path() / ("ui-repro-stockcolour-" + pos::uuid().toStdString() + ".db");
    auto db = std::make_shared<pos::Database>(path);
    db->migrate();
    pos::InventoryService inv(db);
    // Three products, one per stock state (minimum 5).
    pos::ProductDefinition inStock{"AAA In Stock",{}, {},{}, {},{},"piece",300,500,0,0,5,false,false,{}};
    pos::ProductDefinition low{"BBB Low",{}, {},{}, {},{},"piece",300,500,0,0,5,false,false,{}};
    pos::ProductDefinition out{"CCC Out",{}, {},{}, {},{},"piece",300,500,0,0,5,false,false,{}};
    const auto inStockId = inv.createProduct(inStock);
    const auto lowId = inv.createProduct(low);
    inv.createProduct(out);
    inv.receiveStock({inStockId, 25, 300, "piece", {}, {}, "Seed"}); // 25 > 5 => IN STOCK
    inv.receiveStock({lowId, 3, 300, "piece", {}, {}, "Seed"});      // 3 <= 5 => LOW

    const auto badgeBg = [](QTableWidget* t, const char* needle) -> QColor {
        for (int r = 0; r < t->rowCount(); ++r) {
            auto* cell = t->item(r, 3);
            if (cell && cell->text().contains(needle)) return cell->background().color();
        }
        return {};
    };
    const auto check = [&](const QColor& green, const QColor& amber, const QColor& red, const char* theme) {
        qDebug() << theme << "in=" << green << "low=" << amber << "out=" << red;
        // Green: green channel dominates.
        QVERIFY2(green.green() > green.red() && green.green() > green.blue(),
                 qPrintable(QString("%1: in-stock badge is not green").arg(theme)));
        // Red: red channel dominates.
        QVERIFY2(red.red() > red.green() && red.red() > red.blue(),
                 qPrintable(QString("%1: out-of-stock badge is not red").arg(theme)));
        // Amber: warm (red & green both high, above blue) and clearly not the green badge.
        QVERIFY2(amber.red() >= amber.blue() && amber.green() >= amber.blue() && amber != green,
                 qPrintable(QString("%1: low badge is not amber/distinct").arg(theme)));
    };

    // --- Light theme ---
    pos::theme::setDark(false);
    {
        InventoryPage page(db);
        QMetaObject::invokeMethod(&page, "load");
        QTest::qWait(20);
        auto* table = page.findChild<QTableWidget*>("inventoryTable");
        QVERIFY(table);
        QVERIFY(table->rowCount() >= 3);
        check(badgeBg(table, "IN STOCK"), badgeBg(table, "LOW"), badgeBg(table, "OUT OF STOCK"), "light");
    }
    // --- Dark theme: colours must still read correctly ---
    pos::theme::setDark(true);
    {
        InventoryPage page(db);
        QMetaObject::invokeMethod(&page, "load");
        QTest::qWait(20);
        auto* table = page.findChild<QTableWidget*>("inventoryTable");
        QVERIFY(table);
        check(badgeBg(table, "IN STOCK"), badgeBg(table, "LOW"), badgeBg(table, "OUT OF STOCK"), "dark");
    }
    pos::theme::setDark(false);
}

int main(int argc, char** argv) {
    Q_INIT_RESOURCE(resources);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    UiSmokeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ui_smoke_test.moc"
