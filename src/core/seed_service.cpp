#include "core/seed_service.h"
#include "core/database.h"
#include "core/customer_service.h"
#include "core/inventory_service.h"
#include "core/settings_service.h"
#include "core/supplier_service.h"

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
    if (rice.isEmpty()) rice = inventory.createProduct({"Demo Rice", "DEMO-RICE", "990000000001", {}, {}, "Demo seed product", "kg", 9000, 11000, 10000, 9500, 5, false, false, {}});
    auto soap = findProduct("DEMO-SOAP");
    if (soap.isEmpty()) soap = inventory.createProduct({"Demo Soap", "DEMO-SOAP", "990000000002", {}, {}, "Demo seed product", "piece", 5000, 7500, 7000, 6500, 3, false, false, {}});
    auto stock = db_->prepare("SELECT stock_quantity FROM products WHERE id=?");
    stock.bind(1, rice);
    if (stock.stepRow() && stock.integer(0) == 0) inventory.receiveStock({rice, 25, 9000, "kg", {}, {}, "Demo seed"});
    auto soapStock = db_->prepare("SELECT stock_quantity FROM products WHERE id=?");
    soapStock.bind(1, soap);
    if (soapStock.stepRow() && soapStock.integer(0) == 0) inventory.receiveStock({soap, 12, 5000, "piece", {}, {}, "Demo seed"});

    SupplierService suppliers(db_);
    auto supplier = db_->prepare("SELECT id FROM suppliers WHERE name=? AND is_archived=0 LIMIT 1");
    supplier.bind(1, "Demo Supplier");
    if (!supplier.stepRow()) suppliers.create({{}, "Demo Supplier", "Support", "03000000000", "Demo address", 0, false});

    CustomerService customers(db_);
    auto customer = db_->prepare("SELECT id FROM customers WHERE name=? AND is_deleted=0 LIMIT 1");
    customer.bind(1, "Demo Customer");
    if (!customer.stepRow()) customers.create({{}, "Demo Customer", "03111111111", 100000, 30, false});
    settings.setValue("seed.demo.version", "1");
}

} // namespace pos
