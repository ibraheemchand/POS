#include "core/seed_service.h"
#include "core/database.h"
#include "core/customer_service.h"
#include "core/inventory_service.h"
#include "core/settings_service.h"
#include "core/supplier_service.h"
#include <random>

namespace pos {

SeedService::SeedService(std::shared_ptr<Database> database) : db_(std::move(database)) {}

void SeedService::seedDemoData() {
    SettingsService settings(db_);
    if (settings.value("seed.demo.version") == "1") return;

    InventoryService inventory(db_);
    auto findProduct = [this](const QString& sku) {
        auto query = db_->prepare("SELECT id FROM products WHERE sku=? AND is_deleted=0");
        query.bind(1, sku);
        return query.stepRow() ? query.text(0) : QString();
    };
    auto rice = findProduct("DEMO-RICE");
    if (rice.isEmpty()) rice = inventory.createProduct({"Demo Basmati Rice", "DEMO-RICE", "990000000001", {}, {}, "Demo seed product", "kg", 9000, 11000, 10000, 9500, 5, false, false, {}});
    auto soap = findProduct("DEMO-SOAP");
    if (soap.isEmpty()) soap = inventory.createProduct({"Demo Beauty Soap", "DEMO-SOAP", "990000000002", {}, {}, "Demo seed product", "piece", 5000, 7500, 7000, 6500, 15, true, true, {}});

    auto stock = db_->prepare("SELECT stock_quantity FROM products WHERE id=?");
    stock.bind(1, rice);
    if (stock.stepRow() && stock.integer(0) == 0) inventory.receiveStock({rice, 25, 9000, "kg", {}, {}, "Demo seed"});
    
    auto soapStock = db_->prepare("SELECT stock_quantity FROM products WHERE id=?");
    soapStock.bind(1, soap);
    if (soapStock.stepRow() && soapStock.integer(0) == 0) inventory.receiveStock({soap, 12, 5000, "piece", "DEMO-B01", QDate::currentDate().addDays(20), "Demo seed"});

    SupplierService suppliers(db_);
    auto supQuery = db_->prepare("SELECT id FROM suppliers WHERE name=? AND is_archived=0 LIMIT 1");
    supQuery.bind(1, "Demo Wholesalers");
    QString supplierId;
    if (supQuery.stepRow()) {
        supplierId = supQuery.text(0);
    } else {
        supplierId = suppliers.create({{}, "Demo Wholesalers", "Tariq Khan", "03001234567", "Circular Road, Lahore", 250000, false});
    }

    CustomerService customers(db_);
    auto custQuery = db_->prepare("SELECT id FROM customers WHERE name=? AND is_deleted=0 LIMIT 1");
    custQuery.bind(1, "Demo Customer");
    QString customerId;
    if (custQuery.stepRow()) {
        customerId = custQuery.text(0);
    } else {
        customerId = customers.create({{}, "Demo Customer", "03111111111", 100000, 30, false});
    }

    // Seed shift session
    QString shiftId = uuid();
    auto now = utcNow();
    auto shiftCheck = db_->prepare("SELECT COUNT(*) FROM shift_sessions WHERE status='open'");
    if (shiftCheck.stepRow() && shiftCheck.integer(0) == 0) {
        auto insertShift = db_->prepare("INSERT INTO shift_sessions(id,opened_at,opening_cash_paisa,status) VALUES(?,?,?,?)");
        insertShift.bind(1, shiftId);
        insertShift.bind(2, now);
        insertShift.bind(3, static_cast<qint64>(500000));
        insertShift.bind(4, "open");
        insertShift.execute();
    } else {
        auto existing = db_->prepare("SELECT id FROM shift_sessions WHERE status='open' LIMIT 1");
        if(existing.stepRow()) shiftId = existing.text(0);
    }

    // Seed 3 distinct days of completed sales for 7-day trend
    auto saleCheck = db_->prepare("SELECT COUNT(*) FROM sales WHERE status!='voided'");
    saleCheck.stepRow();
    if (saleCheck.integer(0) == 0) {
        struct SeedSale {
            QString inv;
            int dayOffset;
            qint64 total;
        };
        const SeedSale demoSales[] = {
            {"INV-DEMO-001", -3, 320000}, // 3 days ago: PKR 3,200.00
            {"INV-DEMO-002", -1, 580000}, // 1 day ago: PKR 5,800.00
            {"INV-DEMO-003", 0, 440000}   // today: PKR 4,400.00
        };
        for (const auto& s : demoSales) {
            const auto saleId = uuid();
            const auto saleTime = QDate::currentDate().addDays(s.dayOffset).toString(Qt::ISODate) + "T14:30:00Z";
            auto insSale = db_->prepare("INSERT INTO sales(id,invoice_no,customer_id,shift_id,status,payment_method,subtotal_paisa,discount_paisa,total_paisa,paid_paisa,due_paisa,note,created_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)");
            insSale.bind(1, saleId);
            insSale.bind(2, s.inv);
            insSale.bind(3, customerId);
            insSale.bind(4, shiftId);
            insSale.bind(5, "completed");
            insSale.bind(6, "cash");
            insSale.bind(7, s.total);
            insSale.bind(8, static_cast<qint64>(0));
            insSale.bind(9, s.total);
            insSale.bind(10, s.total);
            insSale.bind(11, static_cast<qint64>(0));
            insSale.bind(12, "Demo Counter Sale");
            insSale.bind(13, saleTime);
            insSale.execute();

            auto insCash = db_->prepare("INSERT INTO cash_transactions(id,shift_id,sale_id,type,amount_paisa,reason,created_at) VALUES(?,?,?,?,?,?,?)");
            insCash.bind(1, uuid());
            insCash.bind(2, shiftId);
            insCash.bind(3, saleId);
            insCash.bind(4, "cash_in");
            insCash.bind(5, s.total);
            insCash.bind(6, "Counter payment for " + s.inv);
            insCash.bind(7, saleTime);
            insCash.execute();
        }
    }

    settings.setValue("seed.demo.version", "1");
}

void SeedService::seedRandomData(int productCount, quint32 seed) {
    if (productCount < 1 || productCount > 1000) {
        throw DatabaseError("random seed product count must be between 1 and 1000");
    }
    InventoryService inventory(db_);
    std::mt19937 generator(seed);
    std::uniform_int_distribution<Money> purchasePrice(100, 100000);
    std::uniform_int_distribution<int> markupPercent(10, 60);
    std::uniform_int_distribution<Quantity> stockQuantity(5, 250);
    std::uniform_int_distribution<int> unitChoice(0, 2);
    const QStringList units{"piece", "kg", "box"};
    for (int index = 0; index < productCount; ++index) {
        const auto sku = QString("RND-%1-%2").arg(seed).arg(index + 1, 4, 10, QChar('0'));
        auto existing = db_->prepare("SELECT id FROM products WHERE sku=? AND is_deleted=0");
        existing.bind(1, sku);
        if (existing.stepRow()) continue;
        const auto purchase = purchasePrice(generator);
        const auto retail = purchase + (purchase * markupPercent(generator)) / 100;
        const auto unit = units.at(unitChoice(generator));
        const auto product = inventory.createProduct({
            QString("Random QA Product %1").arg(index + 1), sku, {}, {}, {},
            "Generated by --seed-random", unit, purchase, retail,
            (purchase * 95) / 100, (purchase * 90) / 100,
            2, false, false, {}});
        inventory.receiveStock({product, stockQuantity(generator), purchase, unit, {}, {}, "Random QA seed"});
    }
}

} // namespace pos
