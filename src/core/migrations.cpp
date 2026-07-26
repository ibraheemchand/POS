#include "core/migrations.h"
#include "core/database.h"
#include <array>

namespace pos {
namespace {
void addMissingColumns(Database& db) {
    const QList<QPair<QString,QStringList>> additions={{"customers",{"phone TEXT","address TEXT","credit_limit_paisa INTEGER NOT NULL DEFAULT 0","payment_terms_days INTEGER NOT NULL DEFAULT 0","balance_paisa INTEGER NOT NULL DEFAULT 0","is_deleted INTEGER NOT NULL DEFAULT 0"}},{"sales",{"warehouse_id TEXT REFERENCES warehouses(id)","shift_id TEXT REFERENCES shift_sessions(id)","discount_paisa INTEGER NOT NULL DEFAULT 0","tax_paisa INTEGER NOT NULL DEFAULT 0","paid_paisa INTEGER NOT NULL DEFAULT 0","due_paisa INTEGER NOT NULL DEFAULT 0","note TEXT","voided_at TEXT","void_reason TEXT"}},{"sale_items",{"batch_id TEXT","unit_name TEXT NOT NULL DEFAULT 'base'","unit_price_paisa INTEGER NOT NULL DEFAULT 0","discount_paisa INTEGER NOT NULL DEFAULT 0","line_total_paisa INTEGER NOT NULL DEFAULT 0"}},{"suppliers",{"contact_person TEXT","phone TEXT","address TEXT","opening_balance_paisa INTEGER NOT NULL DEFAULT 0","balance_paisa INTEGER NOT NULL DEFAULT 0","is_archived INTEGER NOT NULL DEFAULT 0"}},{"products",{"sku TEXT","barcode TEXT","category_id TEXT","brand_id TEXT","description TEXT NOT NULL DEFAULT ''","track_batches INTEGER NOT NULL DEFAULT 0","track_expiry INTEGER NOT NULL DEFAULT 0","purchase_price_paisa INTEGER NOT NULL DEFAULT 0","retail_price_paisa INTEGER NOT NULL DEFAULT 0","wholesale_price_paisa INTEGER NOT NULL DEFAULT 0","dealer_price_paisa INTEGER NOT NULL DEFAULT 0","stock_quantity INTEGER NOT NULL DEFAULT 0","minimum_stock INTEGER NOT NULL DEFAULT 0","image_path TEXT","is_deleted INTEGER NOT NULL DEFAULT 0"}},{"purchases",{"warehouse_id TEXT","subtotal_paisa INTEGER NOT NULL DEFAULT 0","discount_paisa INTEGER NOT NULL DEFAULT 0","tax_paisa INTEGER NOT NULL DEFAULT 0","paid_paisa INTEGER NOT NULL DEFAULT 0","due_paisa INTEGER NOT NULL DEFAULT 0","notes TEXT"}},{"cash_transactions",{"drawer_id TEXT","shift_id TEXT","sale_id TEXT","reason TEXT"}},{"cheques",{"party_id TEXT","bank TEXT"}},{"customer_payments",{"note TEXT"}},{"customer_ledger",{"sale_id TEXT"}},{"batches",{"expiry_date TEXT"}}};
    for(const auto& table:additions){QStringList existing;auto info=db.prepare(("PRAGMA table_info("+table.first+")").toUtf8().constData());while(info.stepRow())existing.append(info.text(1));for(const auto& definition:table.second){const auto column=definition.section(' ',0,0);if(!existing.contains(column))db.exec(("ALTER TABLE "+table.first+" ADD COLUMN "+definition).toUtf8().constData());}}
}
}
void applyMigrations(Database& db) {
    db.exec("CREATE TABLE IF NOT EXISTS schema_version(version INTEGER NOT NULL);");
    qint64 version{};
    { auto versionQuery=db.prepare("SELECT COALESCE(MAX(version),0) FROM schema_version"); versionQuery.stepRow(); version=versionQuery.integer(0); }
    static constexpr std::array migrations = {
R"SQL(
CREATE TABLE settings (key TEXT PRIMARY KEY, value TEXT NOT NULL, updated_at TEXT NOT NULL);
CREATE TABLE categories (id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE, created_at TEXT NOT NULL, deleted_at TEXT);
CREATE TABLE brands (id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE, created_at TEXT NOT NULL, deleted_at TEXT);
CREATE TABLE products (id TEXT PRIMARY KEY, name TEXT NOT NULL, sku TEXT UNIQUE, barcode TEXT UNIQUE, category_id TEXT REFERENCES categories(id), brand_id TEXT REFERENCES brands(id), base_unit TEXT NOT NULL, track_batches INTEGER NOT NULL DEFAULT 0, purchase_price_paisa INTEGER NOT NULL DEFAULT 0, retail_price_paisa INTEGER NOT NULL DEFAULT 0, wholesale_price_paisa INTEGER NOT NULL DEFAULT 0, dealer_price_paisa INTEGER NOT NULL DEFAULT 0, stock_quantity INTEGER NOT NULL DEFAULT 0, minimum_stock INTEGER NOT NULL DEFAULT 0, is_deleted INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL, updated_at TEXT NOT NULL);
CREATE INDEX products_lookup ON products(name, barcode, sku);
CREATE TABLE product_units (id TEXT PRIMARY KEY, product_id TEXT NOT NULL REFERENCES products(id), name TEXT NOT NULL, factor INTEGER NOT NULL CHECK(factor > 0), UNIQUE(product_id,name));
CREATE TABLE batches (id TEXT PRIMARY KEY, product_id TEXT NOT NULL REFERENCES products(id), batch_no TEXT NOT NULL, expiry_date TEXT, quantity_remaining INTEGER NOT NULL, purchase_price_paisa INTEGER NOT NULL, created_at TEXT NOT NULL);
CREATE INDEX batches_fefo ON batches(product_id, expiry_date, quantity_remaining);
CREATE TABLE customers (id TEXT PRIMARY KEY, name TEXT NOT NULL, phone TEXT, credit_limit_paisa INTEGER NOT NULL DEFAULT 0, payment_terms_days INTEGER NOT NULL DEFAULT 0, balance_paisa INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL, is_deleted INTEGER NOT NULL DEFAULT 0);
CREATE TABLE suppliers (id TEXT PRIMARY KEY, name TEXT NOT NULL, phone TEXT, balance_paisa INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL, is_deleted INTEGER NOT NULL DEFAULT 0);
CREATE TABLE shift_sessions (id TEXT PRIMARY KEY, opened_at TEXT NOT NULL, closed_at TEXT, opening_cash_paisa INTEGER NOT NULL, expected_cash_paisa INTEGER, counted_cash_paisa INTEGER, status TEXT NOT NULL);
CREATE TABLE sales (id TEXT PRIMARY KEY, invoice_no TEXT NOT NULL UNIQUE, customer_id TEXT REFERENCES customers(id), shift_id TEXT REFERENCES shift_sessions(id), status TEXT NOT NULL, payment_method TEXT NOT NULL, subtotal_paisa INTEGER NOT NULL, discount_paisa INTEGER NOT NULL, total_paisa INTEGER NOT NULL, paid_paisa INTEGER NOT NULL, due_paisa INTEGER NOT NULL, note TEXT, created_at TEXT NOT NULL, voided_at TEXT, void_reason TEXT);
CREATE INDEX sales_dates ON sales(created_at, status);
CREATE TABLE sale_items (id TEXT PRIMARY KEY, sale_id TEXT NOT NULL REFERENCES sales(id), product_id TEXT NOT NULL REFERENCES products(id), batch_id TEXT REFERENCES batches(id), quantity INTEGER NOT NULL, unit_name TEXT NOT NULL, unit_price_paisa INTEGER NOT NULL, discount_paisa INTEGER NOT NULL, line_total_paisa INTEGER NOT NULL);
CREATE TABLE stock_movements (id TEXT PRIMARY KEY, product_id TEXT NOT NULL REFERENCES products(id), batch_id TEXT REFERENCES batches(id), type TEXT NOT NULL, quantity INTEGER NOT NULL, original_unit TEXT, reference_id TEXT, reason TEXT, balance_after INTEGER NOT NULL, performed_by TEXT, created_at TEXT NOT NULL);
CREATE INDEX movements_product_date ON stock_movements(product_id, created_at);
CREATE TABLE customer_ledger (id TEXT PRIMARY KEY, customer_id TEXT NOT NULL REFERENCES customers(id), sale_id TEXT REFERENCES sales(id), description TEXT NOT NULL, debit_paisa INTEGER NOT NULL DEFAULT 0, credit_paisa INTEGER NOT NULL DEFAULT 0, running_balance_paisa INTEGER NOT NULL, created_at TEXT NOT NULL);
CREATE TABLE cash_transactions (id TEXT PRIMARY KEY, shift_id TEXT REFERENCES shift_sessions(id), sale_id TEXT REFERENCES sales(id), type TEXT NOT NULL, amount_paisa INTEGER NOT NULL, reason TEXT, created_at TEXT NOT NULL);
CREATE TABLE audit_log (id TEXT PRIMARY KEY, action TEXT NOT NULL, entity_type TEXT NOT NULL, entity_id TEXT NOT NULL, detail TEXT NOT NULL, created_at TEXT NOT NULL);
)SQL",
R"SQL(
CREATE TABLE backups (id TEXT PRIMARY KEY, file_path TEXT NOT NULL, sha256 TEXT NOT NULL, verified_at TEXT, status TEXT NOT NULL, created_at TEXT NOT NULL);
CREATE TABLE cheques (id TEXT PRIMARY KEY, direction TEXT NOT NULL, party_id TEXT, cheque_no TEXT NOT NULL, bank TEXT, amount_paisa INTEGER NOT NULL, due_date TEXT, status TEXT NOT NULL, created_at TEXT NOT NULL);
CREATE TABLE fbr_queue (id TEXT PRIMARY KEY, sale_id TEXT NOT NULL REFERENCES sales(id), status TEXT NOT NULL DEFAULT 'pending', response TEXT, attempts INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL, synced_at TEXT);
)SQL",
R"SQL(
CREATE TABLE units (id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE, symbol TEXT NOT NULL UNIQUE, created_at TEXT NOT NULL, deleted_at TEXT);
ALTER TABLE products ADD COLUMN image_path TEXT;
CREATE INDEX products_inventory_filter ON products(category_id, brand_id, is_deleted, stock_quantity);
CREATE INDEX batches_expiry ON batches(expiry_date, quantity_remaining);
)SQL",
R"SQL(
ALTER TABLE products ADD COLUMN description TEXT NOT NULL DEFAULT '';
ALTER TABLE products ADD COLUMN track_expiry INTEGER NOT NULL DEFAULT 0;
)SQL",
R"SQL(
CREATE TABLE warehouses (id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE, address TEXT NOT NULL DEFAULT '', is_archived INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL);
CREATE TABLE product_images (id TEXT PRIMARY KEY, product_id TEXT NOT NULL REFERENCES products(id), file_path TEXT NOT NULL, thumbnail_path TEXT, is_primary INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL);
CREATE TABLE inventory (id TEXT PRIMARY KEY, product_id TEXT NOT NULL REFERENCES products(id), warehouse_id TEXT NOT NULL REFERENCES warehouses(id), quantity INTEGER NOT NULL DEFAULT 0, average_cost_paisa INTEGER NOT NULL DEFAULT 0, updated_at TEXT NOT NULL, UNIQUE(product_id,warehouse_id));
CREATE TABLE inventory_snapshots (id TEXT PRIMARY KEY, warehouse_id TEXT REFERENCES warehouses(id), captured_at TEXT NOT NULL, captured_by TEXT, notes TEXT);
CREATE TABLE inventory_snapshot_items (id TEXT PRIMARY KEY, snapshot_id TEXT NOT NULL REFERENCES inventory_snapshots(id), product_id TEXT NOT NULL REFERENCES products(id), expected_quantity INTEGER NOT NULL, counted_quantity INTEGER NOT NULL, UNIQUE(snapshot_id,product_id));
CREATE TABLE inventory_adjustments (id TEXT PRIMARY KEY, product_id TEXT NOT NULL REFERENCES products(id), warehouse_id TEXT REFERENCES warehouses(id), batch_id TEXT REFERENCES batches(id), movement_id TEXT NOT NULL REFERENCES stock_movements(id), adjustment_type TEXT NOT NULL, quantity_delta INTEGER NOT NULL, reason TEXT NOT NULL, created_at TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS suppliers (id TEXT PRIMARY KEY, name TEXT NOT NULL, contact_person TEXT, phone TEXT, address TEXT, opening_balance_paisa INTEGER NOT NULL DEFAULT 0, balance_paisa INTEGER NOT NULL DEFAULT 0, is_archived INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL);
CREATE TABLE supplier_ledger (id TEXT PRIMARY KEY, supplier_id TEXT NOT NULL REFERENCES suppliers(id), reference_id TEXT, entry_type TEXT NOT NULL, debit_paisa INTEGER NOT NULL DEFAULT 0, credit_paisa INTEGER NOT NULL DEFAULT 0, balance_paisa INTEGER NOT NULL, created_at TEXT NOT NULL);
CREATE TABLE customer_addresses (id TEXT PRIMARY KEY, customer_id TEXT NOT NULL REFERENCES customers(id), label TEXT, address TEXT NOT NULL, is_default INTEGER NOT NULL DEFAULT 0);
CREATE TABLE IF NOT EXISTS customers (id TEXT PRIMARY KEY, name TEXT NOT NULL, phone TEXT, address TEXT, credit_limit_paisa INTEGER NOT NULL DEFAULT 0, payment_terms_days INTEGER NOT NULL DEFAULT 0, balance_paisa INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL, is_deleted INTEGER NOT NULL DEFAULT 0);
CREATE TABLE purchases (id TEXT PRIMARY KEY, invoice_no TEXT NOT NULL UNIQUE, supplier_id TEXT NOT NULL REFERENCES suppliers(id), warehouse_id TEXT REFERENCES warehouses(id), status TEXT NOT NULL, subtotal_paisa INTEGER NOT NULL, discount_paisa INTEGER NOT NULL DEFAULT 0, tax_paisa INTEGER NOT NULL DEFAULT 0, total_paisa INTEGER NOT NULL, paid_paisa INTEGER NOT NULL DEFAULT 0, due_paisa INTEGER NOT NULL DEFAULT 0, purchased_at TEXT NOT NULL, notes TEXT);
CREATE TABLE purchase_items (id TEXT PRIMARY KEY, purchase_id TEXT NOT NULL REFERENCES purchases(id), product_id TEXT NOT NULL REFERENCES products(id), batch_id TEXT REFERENCES batches(id), quantity INTEGER NOT NULL, unit_name TEXT NOT NULL, unit_price_paisa INTEGER NOT NULL, discount_paisa INTEGER NOT NULL DEFAULT 0, tax_paisa INTEGER NOT NULL DEFAULT 0, line_total_paisa INTEGER NOT NULL);
CREATE TABLE purchase_returns (id TEXT PRIMARY KEY, purchase_id TEXT NOT NULL REFERENCES purchases(id), supplier_id TEXT NOT NULL REFERENCES suppliers(id), status TEXT NOT NULL, total_paisa INTEGER NOT NULL, returned_at TEXT NOT NULL, reason TEXT NOT NULL);
CREATE TABLE purchase_return_items (id TEXT PRIMARY KEY, return_id TEXT NOT NULL REFERENCES purchase_returns(id), purchase_item_id TEXT REFERENCES purchase_items(id), product_id TEXT NOT NULL REFERENCES products(id), batch_id TEXT REFERENCES batches(id), quantity INTEGER NOT NULL, amount_paisa INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS sales (id TEXT PRIMARY KEY, invoice_no TEXT NOT NULL UNIQUE, customer_id TEXT REFERENCES customers(id), warehouse_id TEXT REFERENCES warehouses(id), shift_id TEXT REFERENCES shift_sessions(id), status TEXT NOT NULL, payment_method TEXT NOT NULL, subtotal_paisa INTEGER NOT NULL, discount_paisa INTEGER NOT NULL DEFAULT 0, tax_paisa INTEGER NOT NULL DEFAULT 0, total_paisa INTEGER NOT NULL, paid_paisa INTEGER NOT NULL DEFAULT 0, due_paisa INTEGER NOT NULL DEFAULT 0, note TEXT, created_at TEXT NOT NULL, voided_at TEXT, void_reason TEXT);
CREATE TABLE IF NOT EXISTS sale_items (id TEXT PRIMARY KEY, sale_id TEXT NOT NULL REFERENCES sales(id), product_id TEXT NOT NULL REFERENCES products(id), batch_id TEXT REFERENCES batches(id), quantity INTEGER NOT NULL, unit_name TEXT NOT NULL, unit_price_paisa INTEGER NOT NULL, discount_paisa INTEGER NOT NULL DEFAULT 0, line_total_paisa INTEGER NOT NULL);
CREATE TABLE sale_returns (id TEXT PRIMARY KEY, sale_id TEXT NOT NULL REFERENCES sales(id), customer_id TEXT REFERENCES customers(id), status TEXT NOT NULL, total_paisa INTEGER NOT NULL, returned_at TEXT NOT NULL, reason TEXT NOT NULL);
CREATE TABLE sale_return_items (id TEXT PRIMARY KEY, return_id TEXT NOT NULL REFERENCES sale_returns(id), sale_item_id TEXT REFERENCES sale_items(id), product_id TEXT NOT NULL REFERENCES products(id), batch_id TEXT REFERENCES batches(id), quantity INTEGER NOT NULL, amount_paisa INTEGER NOT NULL);
CREATE TABLE suspended_sales (id TEXT PRIMARY KEY, label TEXT NOT NULL, customer_id TEXT REFERENCES customers(id), cart_json TEXT NOT NULL, created_at TEXT NOT NULL, expires_at TEXT);
CREATE TABLE expense_categories (id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE, is_archived INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL);
CREATE TABLE expenses (id TEXT PRIMARY KEY, category_id TEXT REFERENCES expense_categories(id), amount_paisa INTEGER NOT NULL, description TEXT NOT NULL, spent_at TEXT NOT NULL, created_at TEXT NOT NULL);
CREATE TABLE cash_drawers (id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE, opening_balance_paisa INTEGER NOT NULL DEFAULT 0, current_balance_paisa INTEGER NOT NULL DEFAULT 0, is_active INTEGER NOT NULL DEFAULT 1, created_at TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS cash_transactions (id TEXT PRIMARY KEY, drawer_id TEXT REFERENCES cash_drawers(id), shift_id TEXT REFERENCES shift_sessions(id), sale_id TEXT REFERENCES sales(id), type TEXT NOT NULL, amount_paisa INTEGER NOT NULL, reason TEXT, created_at TEXT NOT NULL);
CREATE TABLE bank_accounts (id TEXT PRIMARY KEY, bank_name TEXT NOT NULL, account_title TEXT NOT NULL, account_number TEXT NOT NULL UNIQUE, balance_paisa INTEGER NOT NULL DEFAULT 0, is_active INTEGER NOT NULL DEFAULT 1, created_at TEXT NOT NULL);
CREATE TABLE bank_transactions (id TEXT PRIMARY KEY, bank_account_id TEXT NOT NULL REFERENCES bank_accounts(id), type TEXT NOT NULL, amount_paisa INTEGER NOT NULL, reference TEXT, occurred_at TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS cheques (id TEXT PRIMARY KEY, direction TEXT NOT NULL, party_id TEXT, cheque_no TEXT NOT NULL, bank TEXT, amount_paisa INTEGER NOT NULL, due_date TEXT, status TEXT NOT NULL, created_at TEXT NOT NULL);
CREATE TABLE roles (id TEXT PRIMARY KEY, name TEXT NOT NULL UNIQUE, description TEXT NOT NULL DEFAULT '', created_at TEXT NOT NULL);
CREATE TABLE permissions (id TEXT PRIMARY KEY, code TEXT NOT NULL UNIQUE, description TEXT NOT NULL DEFAULT '');
CREATE TABLE role_permissions (role_id TEXT NOT NULL REFERENCES roles(id), permission_id TEXT NOT NULL REFERENCES permissions(id), PRIMARY KEY(role_id,permission_id));
CREATE TABLE users (id TEXT PRIMARY KEY, username TEXT NOT NULL UNIQUE, display_name TEXT NOT NULL, password_hash TEXT NOT NULL, role_id TEXT REFERENCES roles(id), is_active INTEGER NOT NULL DEFAULT 1, created_at TEXT NOT NULL, last_login_at TEXT);
CREATE TABLE notifications (id TEXT PRIMARY KEY, type TEXT NOT NULL, title TEXT NOT NULL, body TEXT NOT NULL, is_read INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL);
CREATE TABLE backup_history (id TEXT PRIMARY KEY, file_path TEXT NOT NULL, checksum TEXT NOT NULL, status TEXT NOT NULL, verified_at TEXT, created_at TEXT NOT NULL);
CREATE INDEX inventory_product_warehouse ON inventory(product_id,warehouse_id); CREATE INDEX supplier_ledger_lookup ON supplier_ledger(supplier_id,created_at); CREATE INDEX purchases_supplier_date ON purchases(supplier_id,purchased_at); CREATE INDEX purchase_items_product ON purchase_items(product_id); CREATE INDEX sales_customer_date ON sales(customer_id,created_at); CREATE INDEX sale_items_product ON sale_items(product_id); CREATE INDEX adjustments_product_date ON inventory_adjustments(product_id,created_at); CREATE INDEX notifications_read_date ON notifications(is_read,created_at);
CREATE VIEW v_inventory_value AS SELECT i.warehouse_id,i.product_id,p.name,i.quantity,i.average_cost_paisa,i.quantity*i.average_cost_paisa AS value_paisa FROM inventory i JOIN products p ON p.id=i.product_id WHERE p.is_deleted=0;
CREATE VIEW v_low_stock AS SELECT id,name,stock_quantity,minimum_stock FROM products WHERE is_deleted=0 AND stock_quantity<=minimum_stock;
CREATE TRIGGER audit_product_archive AFTER UPDATE OF is_deleted ON products WHEN NEW.is_deleted=1 BEGIN INSERT INTO audit_log(id,action,entity_type,entity_id,detail,created_at) VALUES(lower(hex(randomblob(16))),'product_archived','product',NEW.id,NEW.name,CURRENT_TIMESTAMP); END;
)SQL",
R"SQL(
ALTER TABLE suppliers ADD COLUMN contact_person TEXT;
ALTER TABLE suppliers ADD COLUMN address TEXT;
ALTER TABLE suppliers ADD COLUMN opening_balance_paisa INTEGER NOT NULL DEFAULT 0;
ALTER TABLE suppliers ADD COLUMN is_archived INTEGER NOT NULL DEFAULT 0;
)SQL",
R"SQL(
CREATE TABLE customer_payments (id TEXT PRIMARY KEY, customer_id TEXT NOT NULL REFERENCES customers(id), method TEXT NOT NULL, amount_paisa INTEGER NOT NULL CHECK(amount_paisa>0), note TEXT, created_at TEXT NOT NULL);
CREATE INDEX customer_payments_lookup ON customer_payments(customer_id,created_at);
)SQL",
R"SQL(
CREATE TABLE customer_payment_allocations (id TEXT PRIMARY KEY, payment_id TEXT NOT NULL REFERENCES customer_payments(id), sale_id TEXT NOT NULL REFERENCES sales(id), amount_paisa INTEGER NOT NULL CHECK(amount_paisa>0), UNIQUE(payment_id,sale_id));
CREATE INDEX customer_payment_allocations_sale ON customer_payment_allocations(sale_id);
)SQL",
R"SQL(
SELECT 1;
)SQL"};
    if (version>0 && static_cast<size_t>(version)<migrations.size()) {
        // The live connection is backed up before any schema change. This remains
        // separate from the application backup catalog because older schemas may
        // not yet contain that catalog table.
        const auto migrationFolder=db.path().parent_path()/"migration-backups";
        std::filesystem::create_directories(migrationFolder);
        const auto migrationBackup=migrationFolder / ("pre-migration-"+utcNow().replace(":","-").toStdString()+".db");
        db.backupTo(migrationBackup);
    }
    for (size_t i=static_cast<size_t>(version); i<migrations.size(); ++i) {
        Transaction tx(db.handle()); db.exec(migrations[i]);
        auto insert=db.prepare("INSERT INTO schema_version(version) VALUES(?)"); insert.bind(1, static_cast<qint64>(i+1)); insert.execute(); tx.commit();
    }
    Transaction compatibility(db.handle());
    addMissingColumns(db);
    db.exec("CREATE INDEX IF NOT EXISTS customers_phone_lookup ON customers(phone); CREATE INDEX IF NOT EXISTS cheques_due_status ON cheques(due_date,status);");
    compatibility.commit();
}
} // namespace pos
