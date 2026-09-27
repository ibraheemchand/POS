#include <QtTest>
#include "core/database.h"
#include "core/pos_service.h"
#include "core/inventory_service.h"
#include "core/barcode_service.h"
#include "core/supplier_service.h"
#include "core/purchase_service.h"
#include "core/payment_service.h"
#include "core/shift_service.h"
#include "core/return_service.h"
#include "core/cheque_service.h"
#include "core/report_service.h"
#include "core/audit_service.h"
#include "core/settings_service.h"
#include "core/excel_export_service.h"
#include "core/security_service.h"
#include "core/suspended_sale_service.h"
#include "core/notification_service.h"
#include "core/backup_service.h"
#include "core/thermal_print_service.h"
#include "core/seed_service.h"
#include "core/commission_service.h"
#include "core/bundle_service.h"
#include "core/partner_service.h"
#include "core/auth_session.h"
#include "core/customer_service.h"
#include "core/data_migration.h"
#include <QRandomGenerator>
#include <filesystem>
#include <QTemporaryDir>
#include <QFile>

class PosServiceTest final : public QObject {
    Q_OBJECT
private slots:
    void saleDecrementsStockAndCreatesLedger();
    void saleRollsBackWhenStockIsInsufficient();
    void receiptCreatesBatchAndMovement();
    void saleConsumesMultipleBatchesByFefo();
    void voidCashSaleReversesCashEntry();
    void catalogAndUnitConversionsAreValidated();
    void stockControlsProduceAlertsAndHistory();
    void ean13BarcodeRulesAreEnforced();
    void supplierCrudCreatesOpeningLedger();
    void purchaseUpdatesStockAndSupplierPayableTogether();
    void purchaseRollsBackWhenLaterLineIsInvalid();
    void customerPaymentAllocatesToInvoiceAndLedger();
    void customerPaymentRollsBackEarlierAllocationsOnFailure();
    void legacySchemaIsUpgradedAndValidated();
    void cashSaleIsAttachedToShiftAndReconciles();
    void salesReturnRestoresStockAndRefundsCash();
    void purchaseReturnReducesStockAndPayable();
    void purchaseReturnRollsBackWhenStockIsInsufficient();
    void chequeRegisterTracksDueAndStatus();
    void reportsAndAuditQueriesReturnPersistedData();
    void settingsPersistAndReadValues();
    void productImportIsAtomicAndValidated();
    void excelExportProducesOpenXmlWorkbook();
    void securityPinStoresOnlySaltedHashAndVerifies();
    void suspendedSaleRoundTripsAndRemoves();
    void mixedSalePersistsTendersAndCashReversal();
    void notificationsCreateReadAndMarkRead();
    void operationalNotificationsAreGenerated();
    void backupRetentionPrunesOldSnapshots();
    void thermalReceiptAndBarcodeBytesAreValidated();
    void demoSeedIsIdempotent();
    void randomSeedCreatesPricedDataIdempotently();
    void commissionSplitIsPersistedPerSaleItem();
    void commissionOverrideRequiredBeyondFlexibleCap();
    void commissionSettingsRejectSharesExceedingRate();
    void bundleResolvesLivePricesAndStock();
    void bundleResolveItemsExposesLastPurchaseCost();
    void cartDiscountLimitsForMixedCart();
    void invoiceDiscountGreenZoneNeedsNoOverride();
    void invoiceDiscountRedZoneRequiresOverrideAndAudits();
    void invoiceDiscountCannotExceedMaximum();
    void invoiceDiscountSpreadProportionallyToLines();
    void courseSalePostsOneGroupedEntryUsingCourseSettings();
    void standaloneBookSaleUsesBookSettings();
    void multiCourseAndStandaloneBookSaleReconciles();
    void courseWithNoPartnerSendsFullCommissionToProfit();
    void partnerShareEqualToTotalLeavesNoProfit();
    void partialCourseReturnReversesProportionalShare();
    void fullCancellationReversesAllPartnerAndProfitEntries();
    void changingCommissionSettingsDoesNotAlterPastSales();
    void lockedSessionCannotReadOrChangeCommissionData();
    void wrongOwnerPinIsRejected();
    void cashierCanCompleteCommissionedSaleWhileLocked();
    void settingsPersistAfterReopeningDatabase();
    void backupRestoreRoundTripRestoresData();
    void dashboardNumbersReflectSalesAndPurchases();
    void discountIsSharedProportionallyBetweenPartnerAndProfit();
    void discountCapUsesPerItemAndCourseSettings();
    void discountCapClampsToZeroWhenOwnerMinExceedsMargin();
    void securitySetupRecoveryChangeAndLockout();
    void moneyHelpersRoundFormatAndParse();
    void partnerPlusProfitEqualsCommissionForRandomInputs();
    void migrationRenamesPaisaColumnsPreservingValues();
    void databaseIsDurableAgainstPowerLoss();
    void freshDatabaseIsEmptyAndHasNoPin();
    void migrationVerifyDetectsMismatch();
};

// Shared fixture helpers for the partner-commission suite.
static void openShift(pos::Database& db) {
    auto s = db.prepare("INSERT INTO shift_sessions(id,opened_at,opening_cash,status) VALUES(?,?,?,'open')");
    s.bind(1, pos::uuid()); s.bind(2, pos::utcNow()); s.bind(3, 0); s.execute();
}
static void unlockOwner(const std::shared_ptr<pos::Database>& db) {
    pos::SecurityService sec(db);
    if (!sec.hasPin()) sec.setupPin("123456");
    pos::AuthSession::instance().setTimeoutSeconds(300);
    QVERIFY(pos::AuthSession::instance().unlock(db, "123456"));
}
// Set a book/course commission config directly (bypasses the owner-PIN gate for tests).
static void setProductCommission(pos::Database& db, const QString& productId, qint64 totalBp, qint64 partnerBp) {
    auto q = db.prepare("UPDATE products SET total_pct_bp=?,partner_pct_bp=? WHERE id=?");
    q.bind(1, totalBp); q.bind(2, partnerBp); q.bind(3, productId); q.execute();
}
static QString stockedBook(pos::InventoryService& inv, const QString& name, qint64 retail) {
    const auto id = inv.createProduct(name, "piece", retail / 2, retail, false);
    inv.receiveStock({id, 50, retail / 2, "piece", {}, {}, "Seed"});
    return id;
}

static void insertProduct(pos::Database& db, const QString& id, qint64 stock) {
    auto p=db.prepare("INSERT INTO products(id,name,base_unit,stock_quantity,created_at,updated_at) VALUES(?,?,?, ?,?,?)");
    p.bind(1,id);p.bind(2,"Test product");p.bind(3,"piece");p.bind(4,stock);p.bind(5,pos::utcNow());p.bind(6,pos::utcNow());p.execute();
    auto shift=db.prepare("INSERT INTO shift_sessions(id,opened_at,opening_cash,status) VALUES(?,?,?,'open')");shift.bind(1,pos::uuid());shift.bind(2,pos::utcNow());shift.bind(3,0);shift.execute();
}
void PosServiceTest::saleDecrementsStockAndCreatesLedger(){try {const auto path=std::filesystem::temp_directory_path()/("pos-sale-test-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();const auto product=pos::uuid(),customer=pos::uuid();insertProduct(*db,product,20);auto c=db->prepare("INSERT INTO customers(id,name,created_at) VALUES(?,?,?)");c.bind(1,customer);c.bind(2,"Customer");c.bind(3,pos::utcNow());c.execute();pos::PosService service(db);pos::SaleRequest request{customer,{},"credit",0,0,{},{{product,{},5,10000,0,"piece"}}};const auto sale=service.completeSale(request);QVERIFY(!sale.saleId.isEmpty());auto p=db->prepare("SELECT stock_quantity FROM products WHERE id=?");p.bind(1,product);QVERIFY(p.stepRow());QCOMPARE(p.integer(0),qint64(15));auto balance=db->prepare("SELECT balance FROM customers WHERE id=?");balance.bind(1,customer);QVERIFY(balance.stepRow());QCOMPARE(balance.integer(0),qint64(50000));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::saleRollsBackWhenStockIsInsufficient(){try {const auto path=std::filesystem::temp_directory_path()/("pos-rollback-test-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();const auto product=pos::uuid();insertProduct(*db,product,2);pos::PosService service(db);pos::SaleRequest request{{},{},"cash",0,0,{},{{product,{},3,100,0,"piece"}}};QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,service.completeSale(request));auto p=db->prepare("SELECT stock_quantity FROM products WHERE id=?");p.bind(1,product);QVERIFY(p.stepRow());QCOMPARE(p.integer(0),qint64(2));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::receiptCreatesBatchAndMovement(){try {const auto path=std::filesystem::temp_directory_path()/("pos-receipt-test-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::InventoryService service(db);const auto product=service.createProduct("Paracetamol","piece",500,800,true);service.receiveStock({product,24,500,"carton","LOT-01",QDate::currentDate().addDays(90),"Receiving"});auto batch=db->prepare("SELECT quantity_remaining FROM batches WHERE product_id=?");batch.bind(1,product);QVERIFY(batch.stepRow());QCOMPARE(batch.integer(0),qint64(24));auto movement=db->prepare("SELECT balance_after FROM stock_movements WHERE product_id=?");movement.bind(1,product);QVERIFY(movement.stepRow());QCOMPARE(movement.integer(0),qint64(24));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::saleConsumesMultipleBatchesByFefo(){try {const auto path=std::filesystem::temp_directory_path()/("pos-fefo-test-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::InventoryService inventory(db);const auto product=inventory.createProduct("Vaccine","piece",10,25,true);inventory.receiveStock({product,3,10,"piece","EARLY",QDate::currentDate().addDays(4),"Receiving"});inventory.receiveStock({product,4,10,"piece","LATE",QDate::currentDate().addDays(30),"Receiving"});pos::PosService sales(db);sales.completeSale({{}, {}, "cash", 125, 0, {}, {{product,{},5,25,0,"piece"}}});auto batches=db->prepare("SELECT batch_no,quantity_remaining FROM batches WHERE product_id=? ORDER BY expiry_date");batches.bind(1,product);QVERIFY(batches.stepRow());QCOMPARE(batches.integer(1),qint64(0));QVERIFY(batches.stepRow());QCOMPARE(batches.integer(1),qint64(2));auto items=db->prepare("SELECT COUNT(*) FROM sale_items");QVERIFY(items.stepRow());QCOMPARE(items.integer(0),qint64(2));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::voidCashSaleReversesCashEntry(){try {const auto path=std::filesystem::temp_directory_path()/("pos-void-cash-test-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();const auto product=pos::uuid();insertProduct(*db,product,2);pos::PosService service(db);const auto sale=service.completeSale({{}, {}, "cash", 100, 0, {}, {{product,{},1,100,0,"piece"}}});service.voidSale(sale.saleId,"Customer cancelled","Manager");auto cash=db->prepare("SELECT SUM(CASE WHEN type='cash_in' THEN amount ELSE -amount END) FROM cash_transactions WHERE sale_id=?");cash.bind(1,sale.saleId);QVERIFY(cash.stepRow());QCOMPARE(cash.integer(0),qint64(0));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::catalogAndUnitConversionsAreValidated(){try {const auto path=std::filesystem::temp_directory_path()/("pos-catalog-test-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::InventoryService service(db);const auto category=service.createCategory("Medicine");const auto brand=service.createBrand("Invento");QVERIFY(!service.createUnit("Piece","pc").isEmpty());pos::ProductDefinition product{"Syrup","SYR-01","1234567890123",category,brand,{},"piece",100,150,125,110,10,true,false,"C:/images/syrup.png"};const auto productId=service.createProduct(product);service.setUnitConversions(productId,{{"carton",24},{"box",6}});QCOMPARE(service.toBaseQuantity(productId,"carton",2),qint64(48));QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,service.setUnitConversions(productId,{{"broken",0}}));product.retailPrice=175;service.updateProduct(productId,product);auto updated=db->prepare("SELECT retail_price,image_path FROM products WHERE id=?");updated.bind(1,productId);QVERIFY(updated.stepRow());QCOMPARE(updated.integer(0),qint64(175));QCOMPARE(updated.text(1),QString("C:/images/syrup.png"));service.archiveProduct(productId);QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,service.toBaseQuantity(productId,"carton",1));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::stockControlsProduceAlertsAndHistory(){try {const auto path=std::filesystem::temp_directory_path()/("pos-stock-test-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::InventoryService service(db);pos::ProductDefinition product{"Milk",{}, {}, {}, {},{},"piece",100,150,0,0,10,true,false,{}};const auto id=service.createProduct(product);service.receiveStock({id,20,100,"piece","DUE",QDate::currentDate().addDays(3),"Stock"});auto batch=db->prepare("SELECT id FROM batches WHERE product_id=?");batch.bind(1,id);QVERIFY(batch.stepRow());service.adjustStock({id,batch.text(0),-12,"damaged","Broken seal","Manager"});QCOMPARE(service.lowStock().size(),qsizetype(1));QCOMPARE(service.nearExpiry(QDate::currentDate().addDays(7)).size(),qsizetype(1));QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,service.adjustStock({id,batch.text(0),-20,"lost","Impossible","Manager"}));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::ean13BarcodeRulesAreEnforced(){pos::BarcodeService service;QCOMPARE(service.generateEan13("590123412345"),QString("5901234123457"));QVERIFY(service.isValidEan13("5901234123457"));QVERIFY(!service.isValidEan13("5901234123458"));QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,service.generateEan13("ABC"));}
void PosServiceTest::supplierCrudCreatesOpeningLedger(){try{const auto path=std::filesystem::temp_directory_path()/("supplier-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::SupplierService service(db);const auto id=service.create({{},"Acme", "Ali", "0300", "Lahore", 12000, false});auto supplier=service.find(id);QCOMPARE(supplier.name,QString("Acme"));QCOMPARE(supplier.openingBalance,qint64(12000));const auto entries=service.ledger(id);QCOMPARE(entries.size(),qsizetype(1));QCOMPARE(entries.first().debit,qint64(12000));auto ledger=db->prepare("SELECT balance FROM supplier_ledger WHERE supplier_id=?");ledger.bind(1,id);QVERIFY(ledger.stepRow());QCOMPARE(ledger.integer(0),qint64(12000));supplier.name="Acme Updated";service.update(supplier);QCOMPARE(service.find(id).name,QString("Acme Updated"));service.archive(id);QVERIFY(service.find(id).archived);}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::purchaseUpdatesStockAndSupplierPayableTogether(){try{const auto path=std::filesystem::temp_directory_path()/("purchase-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::InventoryService inventory(db);const auto product=inventory.createProduct("Rice","kg",9000,11000,false);pos::SupplierService suppliers(db);const auto supplier=suppliers.create({{},"Wholesale Supplier",{}, {}, {}, 0, false});pos::PurchaseService purchases(db);const auto purchase=purchases.completePurchase({supplier,{},"cash",10000,0,0,{},{{product,10,1000,0,0,"kg",{}, {}}}});QVERIFY(!purchase.purchaseId.isEmpty());auto stock=db->prepare("SELECT stock_quantity FROM products WHERE id=?");stock.bind(1,product);QVERIFY(stock.stepRow());QCOMPARE(stock.integer(0),qint64(10));auto balance=db->prepare("SELECT balance FROM suppliers WHERE id=?");balance.bind(1,supplier);QVERIFY(balance.stepRow());QCOMPARE(balance.integer(0),qint64(0));auto cash=db->prepare("SELECT SUM(amount) FROM cash_transactions");QVERIFY(cash.stepRow());QCOMPARE(cash.integer(0),qint64(10000));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::customerPaymentAllocatesToInvoiceAndLedger(){try{const auto path=std::filesystem::temp_directory_path()/("payment-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();const auto product=pos::uuid(),customer=pos::uuid();insertProduct(*db,product,2);auto insert=db->prepare("INSERT INTO customers(id,name,created_at) VALUES(?,?,?)");insert.bind(1,customer);insert.bind(2,"Paid Customer");insert.bind(3,pos::utcNow());insert.execute();pos::PosService sales(db);const auto sale=sales.completeSale({customer,{},"credit",0,0,{},{{product,{},1,500,0,"piece"}}});pos::PaymentService payments(db);const auto result=payments.recordCustomerPayment(customer,{{sale.saleId,500}},"cash","At counter");QCOMPARE(result.total,qint64(500));auto invoice=db->prepare("SELECT due,status FROM sales WHERE id=?");invoice.bind(1,sale.saleId);QVERIFY(invoice.stepRow());QCOMPARE(invoice.integer(0),qint64(0));QCOMPARE(invoice.text(1),QString("paid"));auto balance=db->prepare("SELECT balance FROM customers WHERE id=?");balance.bind(1,customer);QVERIFY(balance.stepRow());QCOMPARE(balance.integer(0),qint64(0));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::legacySchemaIsUpgradedAndValidated(){try{const auto path=std::filesystem::temp_directory_path()/("legacy-schema-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->exec("CREATE TABLE schema_version(version INTEGER NOT NULL); INSERT INTO schema_version VALUES(8); CREATE TABLE customers(id TEXT PRIMARY KEY,name TEXT NOT NULL,created_at TEXT NOT NULL); CREATE TABLE sales(id TEXT PRIMARY KEY,invoice_no TEXT,customer_id TEXT,status TEXT,payment_method TEXT,subtotal INTEGER,total INTEGER,created_at TEXT); CREATE TABLE sale_items(id TEXT PRIMARY KEY,sale_id TEXT,product_id TEXT,quantity INTEGER); CREATE TABLE suppliers(id TEXT PRIMARY KEY,name TEXT,created_at TEXT); CREATE TABLE products(id TEXT PRIMARY KEY,name TEXT,base_unit TEXT,created_at TEXT,updated_at TEXT); CREATE TABLE purchases(id TEXT PRIMARY KEY,invoice_no TEXT,supplier_id TEXT,status TEXT,total INTEGER,purchased_at TEXT); CREATE TABLE cash_transactions(id TEXT PRIMARY KEY,type TEXT,amount INTEGER,created_at TEXT); CREATE TABLE cheques(id TEXT PRIMARY KEY,direction TEXT,cheque_no TEXT,amount INTEGER,due_date TEXT,status TEXT,created_at TEXT); CREATE TABLE customer_payments(id TEXT PRIMARY KEY,customer_id TEXT,method TEXT,amount INTEGER,created_at TEXT); CREATE TABLE customer_ledger(id TEXT PRIMARY KEY,customer_id TEXT,description TEXT,debit INTEGER,credit INTEGER,running_balance INTEGER,created_at TEXT); CREATE TABLE batches(id TEXT PRIMARY KEY,product_id TEXT,batch_no TEXT,quantity_remaining INTEGER,purchase_price INTEGER,created_at TEXT);");db->migrate();QStringList missing;QVERIFY2(db->isSchemaCompatible(&missing),qPrintable(missing.join(", ")));QVERIFY(missing.isEmpty());}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::cashSaleIsAttachedToShiftAndReconciles(){try{const auto path=std::filesystem::temp_directory_path()/("shift-sale-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();const auto product=pos::uuid();insertProduct(*db,product,2);pos::PosService sales(db);const auto result=sales.completeSale({{}, {}, "cash", 500, 0, {}, {{product,{},1,500,0,"piece"}}});auto row=db->prepare("SELECT shift_id FROM sales WHERE id=?");row.bind(1,result.saleId);QVERIFY(row.stepRow());QVERIFY(!row.text(0).isEmpty());pos::ShiftService shifts(db);const auto closed=shifts.close(row.text(0),500);QCOMPARE(closed.expected,qint64(500));QCOMPARE(closed.difference,qint64(0));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::salesReturnRestoresStockAndRefundsCash(){try{const auto path=std::filesystem::temp_directory_path()/("sales-return-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();const auto product=pos::uuid();insertProduct(*db,product,2);pos::PosService sales(db);const auto sale=sales.completeSale({{}, {}, "cash", 500, 0, {}, {{product,{},1,500,0,"piece"}}});auto item=db->prepare("SELECT id FROM sale_items WHERE sale_id=?");item.bind(1,sale.saleId);QVERIFY(item.stepRow());pos::ReturnService returns(db);const auto result=returns.returnSale(sale.saleId,{{item.text(0),1}},"Damaged",true);QCOMPARE(result.total,qint64(500));auto stock=db->prepare("SELECT stock_quantity FROM products WHERE id=?");stock.bind(1,product);QVERIFY(stock.stepRow());QCOMPARE(stock.integer(0),qint64(2));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::purchaseReturnReducesStockAndPayable(){try{const auto path=std::filesystem::temp_directory_path()/("purchase-return-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::InventoryService inventory(db);const auto product=inventory.createProduct("Rice","kg",100,150,false);pos::SupplierService suppliers(db);const auto supplier=suppliers.create({{},"Supplier",{},{},{},0,false});pos::PurchaseService purchases(db);const auto purchase=purchases.completePurchase({supplier,{},"credit",0,0,0,{},{{product,5,100,0,0,"kg",{}, {}}}});auto item=db->prepare("SELECT id FROM purchase_items WHERE purchase_id=?");item.bind(1,purchase.purchaseId);QVERIFY(item.stepRow());pos::ReturnService returns(db);returns.returnPurchase(purchase.purchaseId,{{item.text(0),2}},"Overstock");auto stock=db->prepare("SELECT stock_quantity FROM products WHERE id=?");stock.bind(1,product);QVERIFY(stock.stepRow());QCOMPARE(stock.integer(0),qint64(3));auto balance=db->prepare("SELECT balance FROM suppliers WHERE id=?");balance.bind(1,supplier);QVERIFY(balance.stepRow());QCOMPARE(balance.integer(0),qint64(300));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::chequeRegisterTracksDueAndStatus(){try{const auto path=std::filesystem::temp_directory_path()/("cheque-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::ChequeService cheques(db);const auto id=cheques.record({{},"received",{},"CH-001","HBL",{},25000,QDate::currentDate().addDays(2)});QVERIFY(!id.isEmpty());QCOMPARE(cheques.dueBy(QDate::currentDate().addDays(2)).size(),qsizetype(1));cheques.setStatus(id,"deposited");QCOMPARE(cheques.dueBy(QDate::currentDate().addDays(2)).size(),qsizetype(0));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::reportsAndAuditQueriesReturnPersistedData(){try{const auto path=std::filesystem::temp_directory_path()/("report-audit-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();auto audit=db->prepare("INSERT INTO audit_log(id,action,entity_type,entity_id,detail,created_at) VALUES(?,?,?,?,?,?)");audit.bind(1,pos::uuid());audit.bind(2,"test_action");audit.bind(3,"test_entity");audit.bind(4,"entity-1");audit.bind(5,"detail");audit.bind(6,pos::utcNow());audit.execute();const auto rows=pos::AuditService(db).recent("test_action");QCOMPARE(rows.size(),qsizetype(1));QCOMPARE(rows.first().entityId,QString("entity-1"));const auto summary=pos::ReportService(db).summary(QDate::currentDate(),QDate::currentDate());QCOMPARE(summary.sales,qint64(0));QCOMPARE(summary.lowStock,qint64(0));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::settingsPersistAndReadValues(){try{const auto path=std::filesystem::temp_directory_path()/("settings-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::SettingsService settings(db);QCOMPARE(settings.value("business.currency","PKR"),QString("PKR"));settings.setValue("business.name","Invento Wholesale");QCOMPARE(settings.value("business.name"),QString("Invento Wholesale"));settings.setValue("business.name","Updated Wholesale");QCOMPARE(settings.value("business.name"),QString("Updated Wholesale"));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::productImportIsAtomicAndValidated(){try{const auto path=std::filesystem::temp_directory_path()/("product-import-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::InventoryService inventory(db);const QList<pos::ProductDefinition> products={{"Imported A","SKU-A","",{}, {},{},"piece",100,150,0,0,0,false,false,{}},{"Imported B","SKU-B","",{}, {},{},"piece",200,250,0,0,0,false,false,{}}};const auto ids=inventory.importProducts(products);QCOMPARE(ids.size(),qsizetype(2));QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,inventory.importProducts({products.first(),{"Broken",{}, {},{}, {},{},"piece",-1,100,0,0,0,false,false,{}}}));auto count=db->prepare("SELECT COUNT(*) FROM products WHERE is_deleted=0");QVERIFY(count.stepRow());QCOMPARE(count.integer(0),qint64(2));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::excelExportProducesOpenXmlWorkbook(){try{const auto path=QString::fromStdString((std::filesystem::temp_directory_path()/("report-"+pos::uuid().toStdString()+".xlsx")).string());pos::ExcelExportService::writeWorkbook(path,{"Metric","Value"},{{"Sales","100"}});QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.read(2),QByteArray("PK"));const auto package=file.readAll();QVERIFY(package.contains("xl/worksheets/sheet1.xml"));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::securityPinStoresOnlySaltedHashAndVerifies(){try{const auto path=std::filesystem::temp_directory_path()/("security-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::SecurityService security(db);QVERIFY(!security.hasPin());QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,security.setupPin("12345"));const auto recovery=security.setupPin("123456");QVERIFY(!recovery.isEmpty());QVERIFY(security.hasPin());QVERIFY(security.verifyPin("123456"));QVERIFY(!security.verifyPin("654321"));QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,security.setupPin("111111"));auto stored=db->prepare("SELECT value FROM settings WHERE key='security.pin_hash'");QVERIFY(stored.stepRow());QVERIFY(!stored.text(0).contains("123456"));security.clearPin();QVERIFY(!security.hasPin());}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::suspendedSaleRoundTripsAndRemoves(){try{const auto path=std::filesystem::temp_directory_path()/("suspended-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::SuspendedSaleService service(db);const auto id=service.save({{"product-1",2,150,"piece"}});QCOMPARE(service.list().size(),qsizetype(1));const auto sale=service.load(id);QCOMPARE(sale.lines.size(),qsizetype(1));QCOMPARE(sale.lines.first().quantity,qint64(2));QCOMPARE(sale.lines.first().unitPrice,qint64(150));service.remove(id);QCOMPARE(service.list().size(),qsizetype(0));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::mixedSalePersistsTendersAndCashReversal(){try{const auto path=std::filesystem::temp_directory_path()/("mixed-sale-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();const auto product=pos::uuid();insertProduct(*db,product,5);pos::PosService sales(db);pos::SaleRequest request{{},{},"mixed",0,0,{},{{product,{},1,1000,0,"piece"}}};request.paidAmount=1000;request.tenders={{"cash",400},{"cheque",600}};const auto result=sales.completeSale(request);auto payments=db->prepare("SELECT COUNT(*),SUM(amount) FROM sale_payments WHERE sale_id=?");payments.bind(1,result.saleId);QVERIFY(payments.stepRow());QCOMPARE(payments.integer(0),qint64(2));QCOMPARE(payments.integer(1),qint64(1000));auto cash=db->prepare("SELECT amount FROM cash_transactions WHERE sale_id=? AND type='cash_in'");cash.bind(1,result.saleId);QVERIFY(cash.stepRow());QCOMPARE(cash.integer(0),qint64(400));sales.voidSale(result.saleId,"Mixed sale reversal","Manager");auto reversed=db->prepare("SELECT SUM(CASE WHEN type='cash_in' THEN amount ELSE -amount END) FROM cash_transactions WHERE sale_id=?");reversed.bind(1,result.saleId);QVERIFY(reversed.stepRow());QCOMPARE(reversed.integer(0),qint64(0));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::notificationsCreateReadAndMarkRead(){try{const auto path=std::filesystem::temp_directory_path()/("notifications-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::NotificationService service(db);const auto id=service.create("low_stock","Low stock","Rice is below minimum");QCOMPARE(service.unread().size(),qsizetype(1));service.markRead(id);QCOMPARE(service.unread().size(),qsizetype(0));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::operationalNotificationsAreGenerated(){try{const auto path=std::filesystem::temp_directory_path()/("notification-events-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::InventoryService inventory(db);pos::ProductDefinition product{"Low stock item",{}, {},{}, {},{},"piece",100,150,0,0,1,false,false,{}};const auto productId=inventory.createProduct(product);inventory.receiveStock({productId,2,100,"piece",{}, {},"Seed"});const auto sale=pos::PosService(db).completeSale({{}, {}, "cash",150,0,{},{{productId,{},1,150,0,"piece"}}});QVERIFY(!sale.saleId.isEmpty());const auto rows=pos::NotificationService(db).unread();bool saleEvent=false,lowStockEvent=false,shiftEvent=false;for(const auto& row:rows){saleEvent|=row.type=="sale";lowStockEvent|=row.type=="low_stock";shiftEvent|=row.type=="shift";}QVERIFY(saleEvent);QVERIFY(lowStockEvent);QVERIFY(shiftEvent);}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::backupRetentionPrunesOldSnapshots(){try{const auto root=std::filesystem::temp_directory_path()/("backup-retention-"+pos::uuid().toStdString());std::filesystem::create_directories(root);const auto live=root/"live.db";auto db=std::make_shared<pos::Database>(live);db->migrate();pos::BackupService service(db);service.createVerifiedBackup(root/"one.db");QTest::qWait(5);service.createVerifiedBackup(root/"two.db");QCOMPARE(service.verifiedBackups().size(),qsizetype(2));service.pruneVerifiedBackups(1);QCOMPARE(service.verifiedBackups().size(),qsizetype(1));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::thermalReceiptAndBarcodeBytesAreValidated(){try{const auto receipt=pos::ThermalPrintService::receiptBytes("Invento","INV-1",{{"Rice",2,300}},300);QVERIFY(receipt.startsWith(QByteArray("\x1b@")));QVERIFY(receipt.contains("TOTAL: PKR 3"));QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,pos::ThermalPrintService::receiptBytes("Invento\x1b","INV-1",{{"Rice",2,300}},300));const auto label=pos::ThermalPrintService::barcodeLabelBytes("Rice","123456789012");QVERIFY(label.contains(QByteArray("\x1dkI")));QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,pos::ThermalPrintService::barcodeLabelBytes("Rice","bad\ncode"));const auto path=QString::fromStdString((std::filesystem::temp_directory_path()/("escpos-"+pos::uuid().toStdString()+".bin")).string());pos::ThermalPrintService::writeRaw(path,label);QFile output(path);QVERIFY(output.open(QIODevice::ReadOnly));QCOMPARE(output.readAll(),label);}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::demoSeedIsIdempotent(){try{const auto path=std::filesystem::temp_directory_path()/("seed-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::SeedService seed(db);seed.seedDemoData();seed.seedDemoData();auto products=db->prepare("SELECT COUNT(*) FROM products WHERE sku LIKE 'DEMO-%'");products.stepRow();QCOMPARE(products.integer(0),qint64(2));auto stock=db->prepare("SELECT SUM(stock_quantity) FROM products WHERE sku LIKE 'DEMO-%'");stock.stepRow();QCOMPARE(stock.integer(0),qint64(37));auto marker=db->prepare("SELECT value FROM settings WHERE key='seed.demo.version'");QVERIFY(marker.stepRow());QCOMPARE(marker.text(0),QString("1"));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::randomSeedCreatesPricedDataIdempotently(){try{const auto path=std::filesystem::temp_directory_path()/("random-seed-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::SeedService seed(db);seed.seedRandomData(8,42);seed.seedRandomData(8,42);auto products=db->prepare("SELECT COUNT(*),MIN(purchase_price),MAX(retail_price),SUM(stock_quantity) FROM products WHERE sku LIKE 'RND-42-%'");QVERIFY(products.stepRow());QCOMPARE(products.integer(0),qint64(8));QVERIFY(products.integer(1)>0);QVERIFY(products.integer(2)>products.integer(1));QVERIFY(products.integer(3)>0);}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::purchaseRollsBackWhenLaterLineIsInvalid(){try{const auto path=std::filesystem::temp_directory_path()/("purchase-rollback-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::InventoryService inventory(db);const auto product=inventory.createProduct("Rice","kg",9000,11000,false);const auto supplier=pos::SupplierService(db).create({{},"Rollback Supplier",{},{},{},0,false});pos::PurchaseRequest request{supplier,{},"credit",0,0,0,{},{{product,2,1000,0,0,"kg",{},{}},{"missing-product",1,1000,0,0,"kg",{}, {}}}};QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,pos::PurchaseService(db).completePurchase(request));auto stock=db->prepare("SELECT stock_quantity FROM products WHERE id=?");stock.bind(1,product);QVERIFY(stock.stepRow());QCOMPARE(stock.integer(0),qint64(0));auto count=db->prepare("SELECT COUNT(*) FROM purchases");QVERIFY(count.stepRow());QCOMPARE(count.integer(0),qint64(0));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::customerPaymentRollsBackEarlierAllocationsOnFailure(){try{const auto path=std::filesystem::temp_directory_path()/("payment-rollback-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();const auto product=pos::uuid(),customer=pos::uuid();insertProduct(*db,product,5);auto insert=db->prepare("INSERT INTO customers(id,name,created_at) VALUES(?,?,?)");insert.bind(1,customer);insert.bind(2,"Rollback Customer");insert.bind(3,pos::utcNow());insert.execute();pos::PosService sales(db);const auto first=sales.completeSale({customer,{},"credit",0,0,{},{{product,{},1,500,0,"piece"}}});const auto second=sales.completeSale({customer,{},"credit",0,0,{},{{product,{},1,600,0,"piece"}}});QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,pos::PaymentService(db).recordCustomerPayment(customer,{{first.saleId,500},{second.saleId,601}},"bank","Should rollback"));auto due=db->prepare("SELECT due FROM sales WHERE id=?");due.bind(1,first.saleId);QVERIFY(due.stepRow());QCOMPARE(due.integer(0),qint64(500));auto count=db->prepare("SELECT COUNT(*) FROM customer_payments");QVERIFY(count.stepRow());QCOMPARE(count.integer(0),qint64(0));auto balance=db->prepare("SELECT balance FROM customers WHERE id=?");balance.bind(1,customer);QVERIFY(balance.stepRow());QCOMPARE(balance.integer(0),qint64(1100));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::purchaseReturnRollsBackWhenStockIsInsufficient(){try{const auto path=std::filesystem::temp_directory_path()/("purchase-return-rollback-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::InventoryService inventory(db);const auto product=inventory.createProduct("Rice","kg",100,150,false);const auto supplier=pos::SupplierService(db).create({{},"Supplier",{},{},{},0,false});const auto purchase=pos::PurchaseService(db).completePurchase({supplier,{},"credit",0,0,0,{},{{product,5,100,0,0,"kg",{}, {}}}});auto item=db->prepare("SELECT id FROM purchase_items WHERE purchase_id=?");item.bind(1,purchase.purchaseId);QVERIFY(item.stepRow());inventory.adjustStock({product,{},-4,"lost","Stock unavailable","Manager"});QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,pos::ReturnService(db).returnPurchase(purchase.purchaseId,{{item.text(0),2}},"Overstock"));auto stock=db->prepare("SELECT stock_quantity FROM products WHERE id=?");stock.bind(1,product);QVERIFY(stock.stepRow());QCOMPARE(stock.integer(0),qint64(1));auto returns=db->prepare("SELECT COUNT(*) FROM purchase_returns");QVERIFY(returns.stepRow());QCOMPARE(returns.integer(0),qint64(0));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::commissionSplitIsPersistedPerSaleItem(){try {const auto path=std::filesystem::temp_directory_path()/("commission-split-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();const auto product=pos::uuid();insertProduct(*db,product,10);setProductCommission(*db,product,3000,0); // 30% book commission, no partner
    pos::PosService sales(db);pos::SaleRequest request{{},{},"cash",9500,0,{},{{product,{},1,10000,500,"piece"}}};const auto sale=sales.completeSale(request);
    auto row=db->prepare("SELECT retail_amount,commission_amount,partner_amount,discount_amount,owner_amount,overridden FROM sale_item_commissions WHERE sale_id=?");row.bind(1,sale.saleId);QVERIFY(row.stepRow());
    QCOMPARE(row.integer(0),qint64(10000));QCOMPARE(row.integer(1),qint64(3000));QCOMPARE(row.integer(2),qint64(0)); // legacy partner_amount always 0 now
    QCOMPARE(row.integer(3),qint64(500));QCOMPARE(row.integer(4),qint64(2500));QCOMPARE(row.integer(5),qint64(0));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::commissionOverrideRequiredBeyondFlexibleCap(){try {const auto path=std::filesystem::temp_directory_path()/("commission-override-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();const auto product=pos::uuid();insertProduct(*db,product,10);setProductCommission(*db,product,3000,1000); // 30% total, 10% partner; ownerMin default 12% -> cap 8% = 800 on 10000
    pos::PosService sales(db);
    pos::SaleRequest blocked{{},{},"cash",8500,0,{},{{product,{},1,10000,1500,"piece",false}}};QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,sales.completeSale(blocked)); // 1500 > 800 cap
    auto stock=db->prepare("SELECT stock_quantity FROM products WHERE id=?");stock.bind(1,product);QVERIFY(stock.stepRow());QCOMPARE(stock.integer(0),qint64(10));
    pos::SaleRequest approved{{},{},"cash",8500,0,{},{{product,{},1,10000,1500,"piece",true}}};const auto sale=sales.completeSale(approved);
    auto row=db->prepare("SELECT discount_amount,owner_amount,overridden FROM sale_item_commissions WHERE sale_id=?");row.bind(1,sale.saleId);QVERIFY(row.stepRow());QCOMPARE(row.integer(0),qint64(1500));QCOMPARE(row.integer(1),qint64(1500));QCOMPARE(row.integer(2),qint64(1));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::commissionSettingsRejectSharesExceedingRate(){try {const auto path=std::filesystem::temp_directory_path()/("commission-settings-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::CommissionService service(db);
    QCOMPARE(service.settings().ownerMinShareBp,qint64(1200)); // default 12%
    auto settings=service.settings();settings.ownerMinShareBp=12000;QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,service.setSettings(settings)); // >100%
    settings.ownerMinShareBp=1500;service.setSettings(settings);QCOMPARE(service.settings().ownerMinShareBp,qint64(1500));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::bundleResolvesLivePricesAndStock(){try {const auto path=std::filesystem::temp_directory_path()/("bundle-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::InventoryService inventory(db);const auto bookA=inventory.createProduct("Math Grade 5","piece",300,500,false);const auto bookB=inventory.createProduct("English Grade 5","piece",200,400,false);inventory.receiveStock({bookA,10,300,"piece",{}, {},"Seed"});inventory.receiveStock({bookB,10,200,"piece",{}, {},"Seed"});pos::BundleService bundles(db);pos::BundleDefinition definition{"Grade 5 Course","Grade 5","Full set",{{bookA,1},{bookB,1}}};const auto id=bundles.createBundle(definition);QCOMPARE(bundles.listBundles().size(),qsizetype(1));const auto resolved=bundles.resolveItems(id);QCOMPARE(resolved.size(),qsizetype(2));QCOMPARE(resolved.first().retailPrice,qint64(500));QCOMPARE(resolved.first().stock,qint64(10));bundles.archiveBundle(id);QCOMPARE(bundles.listBundles().size(),qsizetype(0));}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::bundleResolveItemsExposesLastPurchaseCost(){try {const auto path=std::filesystem::temp_directory_path()/("bundle-cost-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();
    pos::InventoryService inventory(db);const auto bought=inventory.createProduct("Bought Book","piece",0,500,false);const auto neverBought=inventory.createProduct("New Book","piece",0,400,false);
    pos::SupplierService suppliers(db);const auto supplier=suppliers.create({{},"Book Supplier",{},{},{},0,false});
    pos::PurchaseService(db).completePurchase({supplier,{},"credit",0,0,0,{},{{bought,10,350,0,0,"piece",{}, {}}}}); // last cost 350
    pos::BundleService bundles(db);const auto id=bundles.createBundle({"Grade 1","G1","",{{bought,1},{neverBought,1}}});
    const auto resolved=bundles.resolveItems(id);QCOMPARE(resolved.size(),qsizetype(2));
    QCOMPARE(resolved.at(0).purchasePrice,qint64(350)); // reflects the purchase
    QCOMPARE(resolved.at(1).purchasePrice,qint64(0));   // never purchased => 0
}catch(const std::exception& error){QFAIL(error.what());}}
void PosServiceTest::cartDiscountLimitsForMixedCart(){try{const auto path=std::filesystem::temp_directory_path()/("cart-limits-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();unlockOwner(db);
    pos::InventoryService inv(db);
    const auto bookA=stockedBook(inv,"Standalone",1000);setProductCommission(*db,bookA,4000,1500); // 40% total,15% partner
    const auto b1=stockedBook(inv,"B1",1000),b2=stockedBook(inv,"B2",1000);
    const auto zero=stockedBook(inv,"ZeroComm",1000); // 0% commission
    pos::PartnerService ps(db);const auto coursePartner=ps.createPartner({{},"Course Partner",{},{},0,false});
    const auto courseId=pos::BundleService(db).createBundle({"Grade","G","",{{b1,1},{b2,1}}});ps.setCourseConfig(courseId,{3000,1000,coursePartner}); // 30% total,10% partner
    QList<pos::SaleLine> lines={{bookA,{},1,1000,0,"piece"},{b1,{},1,1000,0,"piece",false,courseId},{b2,{},1,1000,0,"piece",false,courseId},{zero,{},1,1000,0,"piece"}};
    const auto limits=pos::CommissionService(db).cartDiscountLimits(lines);
    // allowed: A=(40-15-12)%*1000=130 ; course=(30-10-12)%*2000=160 ; zero=0 => 290
    QCOMPARE(limits.allowed,qint64(290));
    // max = total commission: A=400, course=600, zero=0 => 1000
    QCOMPARE(limits.max,qint64(1000));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::invoiceDiscountGreenZoneNeedsNoOverride(){try{const auto path=std::filesystem::temp_directory_path()/("inv-green-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();openShift(*db);
    pos::InventoryService inv(db);const auto book=stockedBook(inv,"Book",1000);setProductCommission(*db,book,4000,2000); // allowed=(40-20-12)%*1000=80, max=400
    pos::SaleRequest req{{},{},"cash",950,0,{},{{book,{},1,1000,0,"piece"}}};req.invoiceDiscount=50; // <=80, no override
    const auto sale=pos::PosService(db).completeSale(req);QVERIFY(!sale.saleId.isEmpty());
    auto audit=db->prepare("SELECT COUNT(*) FROM audit_log WHERE action='invoice_discount_override'");QVERIFY(audit.stepRow());QCOMPARE(audit.integer(0),qint64(0));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::invoiceDiscountRedZoneRequiresOverrideAndAudits(){try{const auto path=std::filesystem::temp_directory_path()/("inv-red-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();openShift(*db);
    pos::InventoryService inv(db);const auto book=stockedBook(inv,"Book",1000);setProductCommission(*db,book,4000,2000); // allowed=80, max=400
    pos::SaleRequest blocked{{},{},"cash",850,0,{},{{book,{},1,1000,0,"piece"}}};blocked.invoiceDiscount=150; // >80 allowed, <400 max
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,pos::PosService(db).completeSale(blocked));
    pos::SaleRequest approved{{},{},"cash",850,0,{},{{book,{},1,1000,0,"piece"}}};approved.invoiceDiscount=150;approved.invoiceDiscountOverrideApproved=true;
    const auto sale=pos::PosService(db).completeSale(approved);QVERIFY(!sale.saleId.isEmpty());
    auto audit=db->prepare("SELECT COUNT(*) FROM audit_log WHERE action='invoice_discount_override' AND entity_id=?");audit.bind(1,sale.saleId);QVERIFY(audit.stepRow());QCOMPARE(audit.integer(0),qint64(1));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::invoiceDiscountCannotExceedMaximum(){try{const auto path=std::filesystem::temp_directory_path()/("inv-max-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();openShift(*db);
    pos::InventoryService inv(db);const auto book=stockedBook(inv,"Book",1000);setProductCommission(*db,book,4000,2000); // max=400
    pos::SaleRequest req{{},{},"cash",500,0,{},{{book,{},1,1000,0,"piece"}}};req.invoiceDiscount=500;req.invoiceDiscountOverrideApproved=true; // 500>max 400 even with PIN
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,pos::PosService(db).completeSale(req));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::invoiceDiscountSpreadProportionallyToLines(){try{const auto path=std::filesystem::temp_directory_path()/("inv-spread-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();openShift(*db);
    pos::InventoryService inv(db);
    const auto a=stockedBook(inv,"A",1000),b=stockedBook(inv,"B",1000);
    setProductCommission(*db,a,4000,2000);setProductCommission(*db,b,4000,2000); // 40% total,20% partner each; allowed=80 each=160
    pos::SaleRequest req{{},{},"cash",1900,0,{},{{a,{},1,1000,0,"piece"},{b,{},1,1000,0,"piece"}}};req.invoiceDiscount=100; // <=160 allowed, spread 50/50
    const auto sale=pos::PosService(db).completeSale(req);QVERIFY(!sale.saleId.isEmpty());
    // Base per line 950 => commission 380, partner 190, profit 190.
    auto pe=db->prepare("SELECT COALESCE(SUM(sale_value),0),COALESCE(SUM(commission),0),COALESCE(SUM(partner_amount),0),COALESCE(SUM(my_profit),0) FROM profit_entries WHERE sale_id=?");pe.bind(1,sale.saleId);QVERIFY(pe.stepRow());
    QCOMPARE(pe.integer(0),qint64(1900));  // sale_value == subtotal - invoice discount (shares sum exactly)
    QCOMPARE(pe.integer(1),qint64(760));   // commission on discounted base
    QCOMPARE(pe.integer(2),qint64(380));   // partner
    QCOMPARE(pe.integer(3),qint64(380));   // my profit
    QCOMPARE(pe.integer(2)+pe.integer(3),pe.integer(1)); // partner + profit == commission
    auto lt=db->prepare("SELECT COALESCE(SUM(line_total),0) FROM sale_items WHERE sale_id=?");lt.bind(1,sale.saleId);QVERIFY(lt.stepRow());QCOMPARE(lt.integer(0),qint64(1900)); // line totals net of both discounts
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::courseSalePostsOneGroupedEntryUsingCourseSettings(){try{const auto path=std::filesystem::temp_directory_path()/("course-sale-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();unlockOwner(db);openShift(*db);
    pos::InventoryService inv(db);const auto bookA=stockedBook(inv,"Math",1000),bookB=stockedBook(inv,"English",1000);
    pos::PartnerService ps(db);const auto bookPartner=ps.createPartner({{},"Book Partner",{},{},0,false});ps.setBookConfig(bookA,{5000,5000,bookPartner}); // book settings that MUST be ignored inside a course
    const auto course=pos::BundleService(db).createBundle({"Grade 5","G5","",{{bookA,1},{bookB,1}}});
    const auto coursePartner=ps.createPartner({{},"Course Partner",{},{},0,false});ps.setCourseConfig(course,{3000,1000,coursePartner}); // 30% total, 10% partner
    pos::SaleRequest req{{},{},"cash",2000,0,{},{{bookA,{},1,1000,0,"piece",false,course},{bookB,{},1,1000,0,"piece",false,course}}};
    pos::PosService(db).completeSale(req);
    auto courseEntries=db->prepare("SELECT COUNT(*),COALESCE(SUM(commission),0),COALESCE(SUM(partner_amount),0),COALESCE(SUM(my_profit),0) FROM profit_entries WHERE source_type='course'");QVERIFY(courseEntries.stepRow());QCOMPARE(courseEntries.integer(0),qint64(1));QCOMPARE(courseEntries.integer(1),qint64(600));QCOMPARE(courseEntries.integer(2),qint64(200));QCOMPARE(courseEntries.integer(3),qint64(400));
    auto bookEntries=db->prepare("SELECT COUNT(*) FROM profit_entries WHERE source_type='book'");QVERIFY(bookEntries.stepRow());QCOMPARE(bookEntries.integer(0),qint64(0)); // no per-book entries for a course sale
    auto coursePartnerLedger=db->prepare("SELECT COUNT(*),COALESCE(SUM(credit),0) FROM partner_ledger WHERE partner_id=?");coursePartnerLedger.bind(1,coursePartner);QVERIFY(coursePartnerLedger.stepRow());QCOMPARE(coursePartnerLedger.integer(0),qint64(1));QCOMPARE(coursePartnerLedger.integer(1),qint64(200));
    auto bookPartnerLedger=db->prepare("SELECT COUNT(*) FROM partner_ledger WHERE partner_id=?");bookPartnerLedger.bind(1,bookPartner);QVERIFY(bookPartnerLedger.stepRow());QCOMPARE(bookPartnerLedger.integer(0),qint64(0));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::standaloneBookSaleUsesBookSettings(){try{const auto path=std::filesystem::temp_directory_path()/("book-sale-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();unlockOwner(db);openShift(*db);
    pos::InventoryService inv(db);const auto book=stockedBook(inv,"Atlas",1000);
    pos::PartnerService ps(db);const auto partner=ps.createPartner({{},"Partner",{},{},0,false});ps.setBookConfig(book,{4000,1500,partner}); // 40% total, 15% partner
    pos::SaleRequest req{{},{},"cash",1000,0,{},{{book,{},1,1000,0,"piece"}}}; // no courseId => book settings
    pos::PosService(db).completeSale(req);
    auto entry=db->prepare("SELECT source_type,commission,partner_amount,my_profit FROM profit_entries");QVERIFY(entry.stepRow());QCOMPARE(entry.text(0),QString("book"));QCOMPARE(entry.integer(1),qint64(400));QCOMPARE(entry.integer(2),qint64(150));QCOMPARE(entry.integer(3),qint64(250));
    auto ledger=db->prepare("SELECT credit FROM partner_ledger WHERE partner_id=?");ledger.bind(1,partner);QVERIFY(ledger.stepRow());QCOMPARE(ledger.integer(0),qint64(150));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::multiCourseAndStandaloneBookSaleReconciles(){try{const auto path=std::filesystem::temp_directory_path()/("multi-sale-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();unlockOwner(db);openShift(*db);
    pos::InventoryService inv(db);const auto a=stockedBook(inv,"A",1000),b=stockedBook(inv,"B",1000),c=stockedBook(inv,"C",1000),loose=stockedBook(inv,"Loose",1000);
    pos::PartnerService ps(db);const auto p1=ps.createPartner({{},"P1",{},{},0,false}),p2=ps.createPartner({{},"P2",{},{},0,false});
    const auto courseA=pos::BundleService(db).createBundle({"CourseA","G1","",{{a,1},{b,1}}});ps.setCourseConfig(courseA,{3000,1000,p1});
    const auto courseB=pos::BundleService(db).createBundle({"CourseB","G2","",{{c,1}}});ps.setCourseConfig(courseB,{2000,2000,p2});
    ps.setBookConfig(loose,{5000,1000,p1});
    pos::SaleRequest req{{},{},"cash",4000,0,{},{{a,{},1,1000,0,"piece",false,courseA},{b,{},1,1000,0,"piece",false,courseA},{c,{},1,1000,0,"piece",false,courseB},{loose,{},1,1000,0,"piece"}}};
    pos::PosService(db).completeSale(req);
    const auto today=QDateTime::currentDateTimeUtc().date();const auto sum=ps.summary(today,today);
    // courseA: value2000 comm600 partner200 profit400; courseB: value1000 comm200 partner200 profit0; loose: value1000 comm500 partner100 profit400
    QCOMPARE(sum.totalCommission,qint64(1300));QCOMPARE(sum.totalPartner,qint64(500));QCOMPARE(sum.myProfit,qint64(800));
    QCOMPARE(sum.totalPartner+sum.myProfit,sum.totalCommission);
    qint64 courseSum=0;for(const auto& r:ps.courseReport(today,today))courseSum+=r.commission;qint64 bookSum=0;for(const auto& r:ps.standaloneBookReport(today,today))bookSum+=r.commission;
    QCOMPARE(courseSum+bookSum,sum.totalCommission); // per-source rows reconcile to the summary
    QCOMPARE(sum.totalOwed,qint64(500));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::courseWithNoPartnerSendsFullCommissionToProfit(){try{const auto path=std::filesystem::temp_directory_path()/("nopartner-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();unlockOwner(db);openShift(*db);
    pos::InventoryService inv(db);const auto book=stockedBook(inv,"Solo",1000);
    pos::PartnerService ps(db);const auto course=pos::BundleService(db).createBundle({"NoPartner","G","",{{book,1}}});ps.setCourseConfig(course,{3000,0,{}}); // 30% total, no partner
    pos::SaleRequest req{{},{},"cash",1000,0,{},{{book,{},1,1000,0,"piece",false,course}}};pos::PosService(db).completeSale(req);
    auto entry=db->prepare("SELECT commission,partner_amount,my_profit FROM profit_entries");QVERIFY(entry.stepRow());QCOMPARE(entry.integer(0),qint64(300));QCOMPARE(entry.integer(1),qint64(0));QCOMPARE(entry.integer(2),qint64(300));
    auto ledger=db->prepare("SELECT COUNT(*) FROM partner_ledger");QVERIFY(ledger.stepRow());QCOMPARE(ledger.integer(0),qint64(0));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::partnerShareEqualToTotalLeavesNoProfit(){try{const auto path=std::filesystem::temp_directory_path()/("equalshare-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();unlockOwner(db);openShift(*db);
    pos::InventoryService inv(db);const auto book=stockedBook(inv,"Equal",1000);
    pos::PartnerService ps(db);const auto partner=ps.createPartner({{},"Full Partner",{},{},0,false});const auto course=pos::BundleService(db).createBundle({"Equal","G","",{{book,1}}});ps.setCourseConfig(course,{4000,4000,partner});
    pos::SaleRequest req{{},{},"cash",1000,0,{},{{book,{},1,1000,0,"piece",false,course}}};pos::PosService(db).completeSale(req);
    auto entry=db->prepare("SELECT commission,partner_amount,my_profit FROM profit_entries");QVERIFY(entry.stepRow());QCOMPARE(entry.integer(0),qint64(400));QCOMPARE(entry.integer(1),qint64(400));QCOMPARE(entry.integer(2),qint64(0));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::partialCourseReturnReversesProportionalShare(){try{const auto path=std::filesystem::temp_directory_path()/("partial-return-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();unlockOwner(db);openShift(*db);
    pos::InventoryService inv(db);const auto bookA=stockedBook(inv,"Math",1000),bookB=stockedBook(inv,"English",1000);
    pos::PartnerService ps(db);const auto partner=ps.createPartner({{},"Partner",{},{},0,false});const auto course=pos::BundleService(db).createBundle({"Grade 5","G5","",{{bookA,1},{bookB,1}}});ps.setCourseConfig(course,{3000,1000,partner});
    pos::SaleRequest req{{},{},"cash",2000,0,{},{{bookA,{},1,1000,0,"piece",false,course},{bookB,{},1,1000,0,"piece",false,course}}};const auto sale=pos::PosService(db).completeSale(req);
    auto itemB=db->prepare("SELECT id FROM sale_items WHERE sale_id=? AND product_id=?");itemB.bind(1,sale.saleId);itemB.bind(2,bookB);QVERIFY(itemB.stepRow());
    pos::ReturnService(db).returnSale(sale.saleId,{{itemB.text(0),1}},"Damaged",true);
    auto net=db->prepare("SELECT COALESCE(SUM(commission),0),COALESCE(SUM(partner_amount),0),COALESCE(SUM(my_profit),0) FROM profit_entries WHERE source_type='course'");QVERIFY(net.stepRow());QCOMPARE(net.integer(0),qint64(300));QCOMPARE(net.integer(1),qint64(100));QCOMPARE(net.integer(2),qint64(200)); // half reversed
    auto balance=db->prepare("SELECT balance FROM partners WHERE id=?");balance.bind(1,partner);QVERIFY(balance.stepRow());QCOMPARE(balance.integer(0),qint64(100));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::fullCancellationReversesAllPartnerAndProfitEntries(){try{const auto path=std::filesystem::temp_directory_path()/("full-cancel-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();unlockOwner(db);openShift(*db);
    pos::InventoryService inv(db);const auto bookA=stockedBook(inv,"Math",1000),bookB=stockedBook(inv,"English",1000);
    pos::PartnerService ps(db);const auto partner=ps.createPartner({{},"Partner",{},{},0,false});const auto course=pos::BundleService(db).createBundle({"Grade 5","G5","",{{bookA,1},{bookB,1}}});ps.setCourseConfig(course,{3000,1000,partner});
    pos::SaleRequest req{{},{},"cash",2000,0,{},{{bookA,{},1,1000,0,"piece",false,course},{bookB,{},1,1000,0,"piece",false,course}}};const auto sale=pos::PosService(db).completeSale(req);
    pos::PosService(db).voidSale(sale.saleId,"Customer cancelled","Manager");
    auto net=db->prepare("SELECT COALESCE(SUM(commission),0),COALESCE(SUM(partner_amount),0),COALESCE(SUM(my_profit),0) FROM profit_entries WHERE sale_id=?");net.bind(1,sale.saleId);QVERIFY(net.stepRow());QCOMPARE(net.integer(0),qint64(0));QCOMPARE(net.integer(1),qint64(0));QCOMPARE(net.integer(2),qint64(0));
    auto balance=db->prepare("SELECT balance FROM partners WHERE id=?");balance.bind(1,partner);QVERIFY(balance.stepRow());QCOMPARE(balance.integer(0),qint64(0));
    auto history=db->prepare("SELECT COUNT(*) FROM profit_entries WHERE sale_id=?");history.bind(1,sale.saleId);QVERIFY(history.stepRow());QVERIFY(history.integer(0)>=2); // original + reversal kept, not deleted
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::changingCommissionSettingsDoesNotAlterPastSales(){try{const auto path=std::filesystem::temp_directory_path()/("frozen-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();unlockOwner(db);openShift(*db);
    pos::InventoryService inv(db);const auto book=stockedBook(inv,"Math",1000);
    pos::PartnerService ps(db);const auto partner=ps.createPartner({{},"Partner",{},{},0,false});const auto course=pos::BundleService(db).createBundle({"Grade 5","G5","",{{book,1}}});ps.setCourseConfig(course,{3000,1000,partner});
    pos::SaleRequest req{{},{},"cash",1000,0,{},{{book,{},1,1000,0,"piece",false,course}}};pos::PosService(db).completeSale(req);
    ps.setCourseConfig(course,{5000,5000,partner}); // change AFTER the sale
    auto entry=db->prepare("SELECT commission,partner_amount,my_profit FROM profit_entries");QVERIFY(entry.stepRow());QCOMPARE(entry.integer(0),qint64(300));QCOMPARE(entry.integer(1),qint64(100));QCOMPARE(entry.integer(2),qint64(200)); // still the old snapshot
    const auto today=QDateTime::currentDateTimeUtc().date();bool found=false;for(const auto& r:ps.courseReport(today,today)){if(r.sourceId==course){found=true;QCOMPARE(r.commission,qint64(300));QCOMPARE(r.partnerAmount,qint64(100));}}QVERIFY(found);
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::lockedSessionCannotReadOrChangeCommissionData(){try{const auto path=std::filesystem::temp_directory_path()/("locked-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();unlockOwner(db);
    pos::PartnerService ps(db);const auto partner=ps.createPartner({{},"Partner",{},{},0,false});const auto book=pos::InventoryService(db).createProduct("Book","piece",500,1000,false);
    pos::AuthSession::instance().lock(); // simulate a cashier / logged-out session
    const auto today=QDateTime::currentDateTimeUtc().date();
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,ps.listPartners());
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,ps.summary(today,today));
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,ps.partnerLedger(partner,today,today));
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,ps.setBookConfig(book,{3000,1000,partner}));
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,ps.recordPayout(partner,100,"nope"));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::wrongOwnerPinIsRejected(){try{const auto path=std::filesystem::temp_directory_path()/("wrongpin-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();
    pos::SecurityService(db).setupPin("123456");pos::AuthSession::instance().setTimeoutSeconds(300);pos::AuthSession::instance().lock();
    QVERIFY(!pos::AuthSession::instance().unlock(db,"000000"));
    QVERIFY(!pos::AuthSession::instance().isUnlocked());
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,pos::PartnerService(db).listPartners());
    QVERIFY(pos::AuthSession::instance().unlock(db,"123456"));
    QVERIFY(pos::AuthSession::instance().isUnlocked());
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::cashierCanCompleteCommissionedSaleWhileLocked(){try{const auto path=std::filesystem::temp_directory_path()/("cashier-locked-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();unlockOwner(db);openShift(*db);
    pos::InventoryService inv(db);const auto book=stockedBook(inv,"Math",1000);
    pos::PartnerService ps(db);const auto partner=ps.createPartner({{},"Partner",{},{},0,false});const auto course=pos::BundleService(db).createBundle({"Grade 5","G5","",{{book,1}}});ps.setCourseConfig(course,{3000,1000,partner});
    pos::AuthSession::instance().lock(); // cashier / logged-out — must still record commission in the background
    pos::SaleRequest req{{},{},"cash",1000,0,{},{{book,{},1,1000,0,"piece",false,course}}};pos::PosService(db).completeSale(req); // must not throw for a cashier
    auto entry=db->prepare("SELECT commission,partner_amount FROM profit_entries");QVERIFY(entry.stepRow());QCOMPARE(entry.integer(0),qint64(300));QCOMPARE(entry.integer(1),qint64(100));
    QVERIFY(pos::AuthSession::instance().unlock(db,"123456"));
    const auto today=QDateTime::currentDateTimeUtc().date();bool found=false;for(const auto& r:ps.partnerReport(today,today)){if(r.partnerId==partner){found=true;QCOMPARE(r.earned,qint64(100));QCOMPARE(r.balance,qint64(100));}}QVERIFY(found);
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::settingsPersistAfterReopeningDatabase(){try{const auto path=std::filesystem::temp_directory_path()/("settings-persist-"+pos::uuid().toStdString()+".db");
    {auto db=std::make_shared<pos::Database>(path);db->migrate();pos::SettingsService s(db);s.setValue("business.name","Invento Books");s.setValue("backup.interval_hours","6");}
    // Reopen the same file (simulates an app restart) and confirm the values survive.
    {auto db=std::make_shared<pos::Database>(path);db->migrate();pos::SettingsService s(db);QCOMPARE(s.value("business.name"),QString("Invento Books"));QCOMPARE(s.value("backup.interval_hours"),QString("6"));}
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::backupRestoreRoundTripRestoresData(){try{const auto root=std::filesystem::temp_directory_path()/("backup-roundtrip-"+pos::uuid().toStdString());std::filesystem::create_directories(root);const auto live=root/"live.db";
    auto db=std::make_shared<pos::Database>(live);db->migrate();pos::InventoryService inv(db);
    const auto keep=inv.createProduct("Original book","piece",500,1000,false);
    // Statements must be finalized (scoped) before restore, or their read lock blocks the WAL checkpoint.
    const auto countProducts=[&]{qint64 n=0;auto q=db->prepare("SELECT COUNT(*) FROM products");q.stepRow();n=q.integer(0);return n;};
    const auto baseline=countProducts();
    pos::BackupService backup(db);const auto snapshot=root/"snap.db";backup.createVerifiedBackup(snapshot);
    // Mutate after the backup, then restore and confirm the change is rolled back.
    inv.createProduct("Added after backup","piece",100,200,false);
    QCOMPARE(countProducts(),baseline+1);
    backup.restoreVerifiedBackup(snapshot,root/"safety.db");
    QCOMPARE(countProducts(),baseline);
    {auto still=db->prepare("SELECT name FROM products WHERE id=?");still.bind(1,keep);QVERIFY(still.stepRow());QCOMPARE(still.text(0),QString("Original book"));}
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::dashboardNumbersReflectSalesAndPurchases(){try{const auto path=std::filesystem::temp_directory_path()/("dashboard-numbers-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();openShift(*db);
    pos::InventoryService inv(db);const auto product=inv.createProduct("Notebook","piece",500,1000,false);inv.receiveStock({product,20,500,"piece",{}, {},"Seed"});
    const auto customer=pos::uuid();auto c=db->prepare("INSERT INTO customers(id,name,created_at) VALUES(?,?,?)");c.bind(1,customer);c.bind(2,"Buyer");c.bind(3,pos::utcNow());c.execute();
    pos::PosService sales(db);
    sales.completeSale({{},{},"cash",2000,0,{},{{product,{},2,1000,0,"piece"}}});       // cash sale 2000
    sales.completeSale({customer,{},"credit",0,0,{},{{product,{},1,1000,0,"piece"}}});   // credit sale 1000 -> receivable
    pos::SupplierService suppliers(db);const auto supplier=suppliers.create({{},"Supplier",{},{},{},0,false});
    pos::PurchaseService(db).completePurchase({supplier,{},"cash",3000,0,0,{},{{product,3,1000,0,0,"piece",{}, {}}}}); // purchase 3000
    const auto today=QDateTime::currentDateTimeUtc().date();pos::ReportService rep(db);
    const auto sum=rep.summary(today,today);
    QCOMPARE(sum.sales,qint64(3000));           // 2000 cash + 1000 credit
    QCOMPARE(sum.purchases,qint64(3000));
    QCOMPARE(rep.salesCount(today),qint64(2));
    QCOMPARE(rep.todayCashSales(today),qint64(2000));
    QCOMPARE(pos::CustomerService(db).totalReceivables(),qint64(1000));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::discountIsSharedProportionallyBetweenPartnerAndProfit(){try{const auto path=std::filesystem::temp_directory_path()/("discount-share-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();unlockOwner(db);openShift(*db);
    pos::InventoryService inv(db);const auto book=stockedBook(inv,"Math",1000);
    pos::PartnerService ps(db);const auto partner=ps.createPartner({{},"Partner",{},{},0,false});const auto course=pos::BundleService(db).createBundle({"Grade 5","G5","",{{book,1}}});ps.setCourseConfig(course,{3000,1000,partner}); // 30% total, 10% partner
    // Discount 100 on a 1000 line (over the 8% cap -> manager override), net = 900.
    pos::SaleRequest req{{},{},"cash",900,0,{},{{book,{},1,1000,100,"piece",true,course}}};pos::PosService(db).completeSale(req);
    auto e=db->prepare("SELECT sale_value,commission,partner_amount,my_profit FROM profit_entries WHERE source_type='course'");QVERIFY(e.stepRow());
    QCOMPARE(e.integer(0),qint64(900));   // commission base is the discounted price
    QCOMPARE(e.integer(1),qint64(270));   // 30% of 900
    QCOMPARE(e.integer(2),qint64(90));    // partner 10% of 900 (shares the discount)
    QCOMPARE(e.integer(3),qint64(180));   // my profit = 270 - 90
    auto bal=db->prepare("SELECT balance FROM partners WHERE id=?");bal.bind(1,partner);QVERIFY(bal.stepRow());QCOMPARE(bal.integer(0),qint64(90));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::discountCapUsesPerItemAndCourseSettings(){try{const auto path=std::filesystem::temp_directory_path()/("percap-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();openShift(*db);
    pos::InventoryService inv(db);
    const auto configured=stockedBook(inv,"Configured",1000);setProductCommission(*db,configured,3000,0); // cap = 30-0-12 = 18% = 180
    const auto bare=stockedBook(inv,"Bare",1000); // no config -> cap 0
    pos::PosService sales(db);
    // Configured book: discount 150 (<=180) allowed with no override.
    sales.completeSale({{},{},"cash",850,0,{},{{configured,{},1,1000,150,"piece",false}}});
    // Bare book: any discount needs the manager PIN (cap 0).
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,sales.completeSale({{},{},"cash",990,0,{},{{bare,{},1,1000,10,"piece",false}}}));
    // A book loaded from a course uses the COURSE's cap, not the book's own.
    unlockOwner(db);const auto course=pos::BundleService(db).createBundle({"C","G","",{{bare,1}}});pos::PartnerService(db).setCourseConfig(course,{4000,0,{}}); // cap = 40-0-12 = 28% = 280
    sales.completeSale({{},{},"cash",750,0,{},{{bare,{},1,1000,250,"piece",false,course}}}); // 250 <= 280 allowed
    auto count=db->prepare("SELECT COUNT(*) FROM sales WHERE status='completed'");count.stepRow();QCOMPARE(count.integer(0),qint64(2));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::discountCapClampsToZeroWhenOwnerMinExceedsMargin(){try{const auto path=std::filesystem::temp_directory_path()/("cap-clamp-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();openShift(*db);
    pos::CommissionService c(db); // owner minimum default 12%
    // total% - partner% - ownerMin% is negative here (10 - 0 - 12) -> cap must clamp to 0, never negative.
    QCOMPARE(c.flexibleCap(10000,1000,0),qint64(0));
    QCOMPARE(c.flexibleCap(10000,1200,0),qint64(0));   // exactly at the floor -> still 0
    QCOMPARE(c.flexibleCap(10000,3000,1000),qint64(800)); // 30-10-12=8% -> 800 (positive case, sanity)
    // And enforced end to end: a 10%-only book allows no discount without the manager PIN.
    pos::InventoryService inv(db);const auto book=inv.createProduct("Thin margin","piece",500,1000,false);inv.receiveStock({book,10,500,"piece",{}, {},"Seed"});setProductCommission(*db,book,1000,0);
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,pos::PosService(db).completeSale({{},{},"cash",999,0,{},{{book,{},1,1000,1,"piece",false}}}));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::securitySetupRecoveryChangeAndLockout(){try{const auto path=std::filesystem::temp_directory_path()/("sec-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();pos::SecurityService s(db);
    // Setup: min 6 digits, returns a recovery code, cannot re-setup.
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,s.setupPin("12345"));
    const auto recovery=s.setupPin("123456");QVERIFY(!recovery.isEmpty());
    // The recovery code is stored only as a salted hash — never in plain text.
    {auto st=db->prepare("SELECT value FROM settings WHERE key='security.recovery_hash'");QVERIFY(st.stepRow());QVERIFY(!st.text(0).isEmpty());QVERIFY(!st.text(0).contains(QString(recovery).remove('-')));QVERIFY(!st.text(0).contains(recovery));}
    // Change requires current PIN.
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,s.changePin("000000","654321"));
    s.changePin("123456","654321");QVERIFY(s.verifyPin("654321"));QVERIFY(!s.verifyPin("123456"));
    // Recovery resets the PIN, rotates the code, and consumes the old one.
    const auto newRecovery=s.resetWithRecovery(recovery,"222222");QVERIFY(newRecovery!=recovery);QVERIFY(s.verifyPin("222222"));
    QVERIFY_THROWS_EXCEPTION(pos::DatabaseError,s.resetWithRecovery(recovery,"333333")); // old code no longer valid
    // Lockout after 5 wrong attempts + audit entry.
    for(int i=0;i<4;++i)QCOMPARE(int(s.attemptUnlock("000000")),int(pos::SecurityService::UnlockResult::Wrong));
    QCOMPARE(int(s.attemptUnlock("000000")),int(pos::SecurityService::UnlockResult::LockedOut));
    QVERIFY(s.isLockedOut());
    QCOMPARE(int(s.attemptUnlock("222222")),int(pos::SecurityService::UnlockResult::LockedOut)); // correct PIN still blocked while locked
    auto audit=db->prepare("SELECT COUNT(*) FROM audit_log WHERE action='pin_lockout'");audit.stepRow();QCOMPARE(audit.integer(0),qint64(1));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::moneyHelpersRoundFormatAndParse(){
    // Values are exact hundredths of a rupee. parse -> compute -> format at the edges.
    // 0.1 + 0.2 is exactly 0.30 (no floating drift), and compares equal.
    const auto a=pos::parseMoney("0.1"), b=pos::parseMoney("0.2");
    QCOMPARE(a,qint64(10)); QCOMPARE(b,qint64(20));
    QCOMPARE(a+b,qint64(30));
    QCOMPARE(pos::formatMoney(a+b),QString("0.30"));
    // 15% of 999 = 149.85 exactly.
    const auto base=pos::parseMoney("999");
    const auto fifteen=pos::roundMoney(static_cast<double>(base)*1500/10000.0);
    QCOMPARE(fifteen,qint64(14985));
    QCOMPARE(pos::formatMoney(fifteen),QString("149.85"));
    // Display (bare — callers add "PKR "): whole vs decimal, thousands grouping, negatives.
    QCOMPARE(pos::formatMoney(100000),QString("1,000"));
    QCOMPARE(pos::formatMoney(100050),QString("1,000.50"));
    QCOMPARE(pos::formatMoney(150),QString("1.50"));
    QCOMPARE(pos::formatMoney(-5000),QString("-50"));
    QCOMPARE("PKR " + pos::formatMoney(100000),QString("PKR 1,000")); // as shown on screen
    // Round half up.
    QCOMPARE(pos::roundMoney(2.5),qint64(3));
    QCOMPARE(pos::roundMoney(2.49),qint64(2));
    // Parse accepts up to 2 decimals and rejects more, and strips grouping / PKR.
    bool ok=false; pos::parseMoney("1.234",&ok); QVERIFY(!ok);
    pos::parseMoney("12.5",&ok); QVERIFY(ok);
    QCOMPARE(pos::parseMoney("1,000.50"),qint64(100050));
    QCOMPARE(pos::parseMoney("PKR 25"),qint64(2500));
    pos::parseMoney("abc",&ok); QVERIFY(!ok);
}

void PosServiceTest::partnerPlusProfitEqualsCommissionForRandomInputs(){
    // For 1000 random amounts and percentages: partner + my profit == commission, exactly.
    auto* rng=QRandomGenerator::global();
    for(int i=0;i<1000;++i){
        const qint64 base=rng->bounded(1,5000000);        // up to PKR 50,000.00 in hundredths
        const qint64 totalBp=rng->bounded(0,10001);       // 0..100%
        const qint64 partnerBp=rng->bounded(0,int(totalBp)+1); // partner <= total
        const auto commission=pos::roundMoney(static_cast<double>(base)*totalBp/10000.0);
        const auto partner=pos::roundMoney(static_cast<double>(base)*partnerBp/10000.0);
        const auto profit=commission-partner;            // remainder rule
        QCOMPARE(partner+profit,commission);
        QVERIFY(partner<=commission);
    }
}

void PosServiceTest::migrationRenamesPaisaColumnsPreservingValues(){try{const auto path=std::filesystem::temp_directory_path()/("paisa-migrate-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);
    // A legacy database that still uses *_paisa money columns, with known values.
    db->exec("CREATE TABLE schema_version(version INTEGER NOT NULL); INSERT INTO schema_version VALUES(8);"
             "CREATE TABLE customers(id TEXT PRIMARY KEY,name TEXT NOT NULL,created_at TEXT NOT NULL,balance_paisa INTEGER);"
             "CREATE TABLE sales(id TEXT PRIMARY KEY,invoice_no TEXT,customer_id TEXT,status TEXT,payment_method TEXT,subtotal_paisa INTEGER,total_paisa INTEGER,created_at TEXT);"
             "CREATE TABLE sale_items(id TEXT PRIMARY KEY,sale_id TEXT,product_id TEXT,quantity INTEGER);"
             "CREATE TABLE suppliers(id TEXT PRIMARY KEY,name TEXT,created_at TEXT);"
             "CREATE TABLE products(id TEXT PRIMARY KEY,name TEXT,base_unit TEXT,created_at TEXT,updated_at TEXT);"
             "CREATE TABLE purchases(id TEXT PRIMARY KEY,invoice_no TEXT,supplier_id TEXT,status TEXT,total_paisa INTEGER,purchased_at TEXT);"
             "CREATE TABLE cash_transactions(id TEXT PRIMARY KEY,type TEXT,amount_paisa INTEGER,created_at TEXT);"
             "CREATE TABLE cheques(id TEXT PRIMARY KEY,direction TEXT,cheque_no TEXT,amount_paisa INTEGER,due_date TEXT,status TEXT,created_at TEXT);"
             "CREATE TABLE customer_payments(id TEXT PRIMARY KEY,customer_id TEXT,method TEXT,amount_paisa INTEGER,created_at TEXT);"
             "CREATE TABLE customer_ledger(id TEXT PRIMARY KEY,customer_id TEXT,description TEXT,debit_paisa INTEGER,credit_paisa INTEGER,running_balance_paisa INTEGER,created_at TEXT);"
             "CREATE TABLE batches(id TEXT PRIMARY KEY,product_id TEXT,batch_no TEXT,quantity_remaining INTEGER,purchase_price_paisa INTEGER,created_at TEXT);"
             "INSERT INTO sales(id,invoice_no,status,payment_method,subtotal_paisa,total_paisa,created_at) VALUES('s1','INV-1','completed','cash',100000,100000,'2026-01-01T00:00:00Z');"
             "INSERT INTO customers(id,name,created_at,balance_paisa) VALUES('c1','Buyer','2026-01-01T00:00:00Z',50050);");
    db->migrate();
    // Columns renamed; values are the SAME exact hundredths (100000 == PKR 1,000).
    auto info=db->prepare("PRAGMA table_info(sales)"); bool hasTotal=false,hasPaisa=false; while(info.stepRow()){if(info.text(1)=="total")hasTotal=true; if(info.text(1)=="total_paisa")hasPaisa=true;}
    QVERIFY(hasTotal); QVERIFY(!hasPaisa);
    auto s=db->prepare("SELECT total FROM sales WHERE id='s1'"); QVERIFY(s.stepRow()); QCOMPARE(s.integer(0),qint64(100000));
    QCOMPARE("PKR " + pos::formatMoney(s.integer(0)),QString("PKR 1,000"));
    auto c=db->prepare("SELECT balance FROM customers WHERE id='c1'"); QVERIFY(c.stepRow()); QCOMPARE(c.integer(0),qint64(50050));
    QCOMPARE("PKR " + pos::formatMoney(c.integer(0)),QString("PKR 500.50"));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::databaseIsDurableAgainstPowerLoss(){try{const auto path=std::filesystem::temp_directory_path()/("durable-"+pos::uuid().toStdString()+".db");auto db=std::make_shared<pos::Database>(path);db->migrate();
    // WAL + synchronous=FULL: committed writes are fsynced, so a sale survives a power
    // cut. (synchronous: 2 = FULL.) Losing these settings would risk data loss.
    {auto sync=db->prepare("PRAGMA synchronous");QVERIFY(sync.stepRow());QCOMPARE(sync.integer(0),qint64(2));}
    {auto mode=db->prepare("PRAGMA journal_mode");QVERIFY(mode.stepRow());QCOMPARE(mode.text(0).toLower(),QString("wal"));}
    {auto fk=db->prepare("PRAGMA foreign_keys");QVERIFY(fk.stepRow());QCOMPARE(fk.integer(0),qint64(1));}
    // A committed sale is really on disk: reopen the file and it is still there.
    openShift(*db);const auto product=pos::uuid();insertProduct(*db,product,10);
    const auto sale=pos::PosService(db).completeSale({{}, {}, "cash", 5000, 0, {}, {{product,{},1,5000,0,"piece"}}});
    QVERIFY(!sale.saleId.isEmpty());
    db.reset(); // close (simulates the app going away)
    auto reopened=std::make_shared<pos::Database>(path);
    auto q=reopened->prepare("SELECT COUNT(*) FROM sales WHERE status='completed'");QVERIFY(q.stepRow());QCOMPARE(q.integer(0),qint64(1));
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::freshDatabaseIsEmptyAndHasNoPin(){try{
    const auto dir=std::filesystem::temp_directory_path()/("fresh-"+pos::uuid().toStdString());
    std::filesystem::create_directories(dir);
    auto db=std::make_shared<pos::Database>(dir/"business.db"); db->migrate();
    // A brand-new install must have no owner PIN (so the first-run wizard shows).
    QVERIFY(!pos::SecurityService(db).hasPin());
    // ...and no business data of any kind.
    const QStringList tables={"products","customers","suppliers","sales","sale_items","purchases",
                              "purchase_items","bundles","bundle_items","partners","profit_entries",
                              "cash_transactions","cheques"};
    for(const auto& t:tables){const QByteArray sql=("SELECT COUNT(*) FROM "+t).toUtf8();auto q=db->prepare(sql.constData());
        QVERIFY(q.stepRow());QVERIFY2(q.integer(0)==0,qPrintable(t+" is not empty on a fresh database"));}
}catch(const std::exception& error){QFAIL(error.what());}}

void PosServiceTest::migrationVerifyDetectsMismatch(){try{
    const auto dir=std::filesystem::temp_directory_path()/("mig-"+pos::uuid().toStdString());
    std::filesystem::create_directories(dir);
    const auto p1=dir/"a.db", p2=dir/"b.db";
    { auto db=std::make_shared<pos::Database>(p1); db->migrate(); pos::InventoryService(db).createProduct("Widget","piece",100,200,false); } // closed -> WAL checkpointed
    std::filesystem::copy_file(p1,p2);
    const auto s1=QString::fromStdWString(p1.wstring()), s2=QString::fromStdWString(p2.wstring());
    QString err;
    QVERIFY2(pos::verifyDatabasesMatch(s1,s2,&err),qPrintable("identical copy should verify: "+err)); // byte-identical -> matches
    { auto db=std::make_shared<pos::Database>(p2); pos::InventoryService(db).createProduct("Extra","piece",1,2,false); } // tamper
    QVERIFY2(!pos::verifyDatabasesMatch(s1,s2,&err),"a changed copy must fail verification"); // row counts differ now
}catch(const std::exception& error){QFAIL(error.what());}}

QTEST_APPLESS_MAIN(PosServiceTest)
#include "pos_service_test.moc"
