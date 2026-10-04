#include "core/migrations.h"
#include "core/database.h"
#include "core/types.h"
#include <array>
#include <fstream>

namespace pos {
namespace {

bool columnExists(Database& db, const QString& table, const QString& column) {
    auto info = db.prepare(("PRAGMA table_info(" + table + ")").toUtf8().constData());
    while (info.stepRow()) if (info.text(1) == column) return true;
    return false;
}
qint64 sumColumn(Database& db, const QString& table, const QString& column) {
    if (!columnExists(db, table, column)) return 0;
    auto q = db.prepare(("SELECT COALESCE(SUM(" + column + "),0) FROM " + table).toUtf8().constData());
    q.stepRow();
    return q.integer(0);
}

// Renames every *_paisa money column to its rupee-facing name (values are exact
// hundredths and are NOT changed — only the name changes). Idempotent: a column is
// renamed only when the old name still exists and the new one does not. On an
// existing database it first takes a timestamped safety backup and writes a
// before/after totals report so the owner can confirm nothing shifted.
void migrateMoneyColumnsToRupees(Database& db, bool existingDb) {
    const QList<QPair<QString, QList<QPair<QString, QString>>>> renames = {
        {"customers", {{"credit_limit_paisa","credit_limit"},{"balance_paisa","balance"}}},
        {"suppliers", {{"opening_balance_paisa","opening_balance"},{"balance_paisa","balance"}}},
        {"products", {{"purchase_price_paisa","purchase_price"},{"retail_price_paisa","retail_price"},{"wholesale_price_paisa","wholesale_price"},{"dealer_price_paisa","dealer_price"}}},
        {"batches", {{"purchase_price_paisa","purchase_price"}}},
        {"sales", {{"subtotal_paisa","subtotal"},{"discount_paisa","discount"},{"tax_paisa","tax"},{"total_paisa","total"},{"paid_paisa","paid"},{"due_paisa","due"}}},
        {"sale_items", {{"unit_price_paisa","unit_price"},{"discount_paisa","discount"},{"line_total_paisa","line_total"}}},
        {"purchases", {{"subtotal_paisa","subtotal"},{"discount_paisa","discount"},{"tax_paisa","tax"},{"total_paisa","total"},{"paid_paisa","paid"},{"due_paisa","due"}}},
        {"purchase_items", {{"unit_price_paisa","unit_price"},{"discount_paisa","discount"},{"tax_paisa","tax"},{"line_total_paisa","line_total"}}},
        {"purchase_returns", {{"total_paisa","total"}}},
        {"purchase_return_items", {{"amount_paisa","amount"}}},
        {"sale_returns", {{"total_paisa","total"}}},
        {"sale_return_items", {{"amount_paisa","amount"}}},
        {"customer_ledger", {{"debit_paisa","debit"},{"credit_paisa","credit"},{"running_balance_paisa","running_balance"}}},
        {"supplier_ledger", {{"debit_paisa","debit"},{"credit_paisa","credit"},{"balance_paisa","balance"}}},
        {"cash_transactions", {{"amount_paisa","amount"}}},
        {"shift_sessions", {{"opening_cash_paisa","opening_cash"},{"expected_cash_paisa","expected_cash"},{"counted_cash_paisa","counted_cash"}}},
        {"cheques", {{"amount_paisa","amount"}}},
        {"expenses", {{"amount_paisa","amount"}}},
        {"cash_drawers", {{"opening_balance_paisa","opening_balance"},{"current_balance_paisa","current_balance"}}},
        {"bank_accounts", {{"balance_paisa","balance"}}},
        {"bank_transactions", {{"amount_paisa","amount"}}},
        {"inventory", {{"average_cost_paisa","average_cost"}}},
        {"sale_payments", {{"amount_paisa","amount"}}},
        {"customer_payments", {{"amount_paisa","amount"}}},
        {"customer_payment_allocations", {{"amount_paisa","amount"}}},
        {"sale_item_commissions", {{"retail_amount_paisa","retail_amount"},{"commission_amount_paisa","commission_amount"},{"partner_amount_paisa","partner_amount"},{"discount_amount_paisa","discount_amount"},{"owner_amount_paisa","owner_amount"}}},
        {"partners", {{"balance_paisa","balance"}}},
        {"partner_ledger", {{"debit_paisa","debit"},{"credit_paisa","credit"},{"running_balance_paisa","running_balance"}}},
        {"profit_entries", {{"sale_value_paisa","sale_value"},{"commission_paisa","commission"},{"partner_amount_paisa","partner_amount"},{"my_profit_paisa","my_profit"}}},
    };

    bool needed = false;
    for (const auto& t : renames) {
        for (const auto& c : t.second) if (columnExists(db, t.first, c.first)) { needed = true; break; }
        if (needed) break;
    }
    if (!needed) return;

    struct Check { const char* label; QString table; QString oldCol; QString newCol; };
    const QList<Check> checks = {
        {"Sales (total)","sales","total_paisa","total"},
        {"Sale item line totals","sale_items","line_total_paisa","line_total"},
        {"Customer balances","customers","balance_paisa","balance"},
        {"Supplier balances","suppliers","balance_paisa","balance"},
        {"Partner balances","partners","balance_paisa","balance"},
        {"Partner ledger credits","partner_ledger","credit_paisa","credit"},
        {"Partner ledger debits","partner_ledger","debit_paisa","debit"},
        {"Profit commission","profit_entries","commission_paisa","commission"},
        {"Profit partner share","profit_entries","partner_amount_paisa","partner_amount"},
        {"My profit","profit_entries","my_profit_paisa","my_profit"},
        {"Customer ledger credits","customer_ledger","credit_paisa","credit"},
        {"Product retail prices","products","retail_price_paisa","retail_price"},
    };
    QList<qint64> before;
    if (existingDb) for (const auto& c : checks) before.append(sumColumn(db, c.table, c.oldCol));

    if (existingDb) {
        const auto folder = db.path().parent_path() / "money-migration-backups";
        std::filesystem::create_directories(folder);
        db.backupTo(folder / ("pre-rupees-" + utcNow().replace(":", "-").toStdString() + ".db"));
    }

    {
        Transaction tx(db.handle());
        db.exec("DROP VIEW IF EXISTS v_inventory_value;");
        for (const auto& t : renames)
            for (const auto& c : t.second)
                if (columnExists(db, t.first, c.first) && !columnExists(db, t.first, c.second))
                    db.exec(("ALTER TABLE " + t.first + " RENAME COLUMN " + c.first + " TO " + c.second).toUtf8().constData());
        db.exec("CREATE VIEW IF NOT EXISTS v_inventory_value AS SELECT i.warehouse_id,i.product_id,p.name,i.quantity,i.average_cost,i.quantity*i.average_cost AS value FROM inventory i JOIN products p ON p.id=i.product_id WHERE p.is_deleted=0;");
        tx.commit();
    }

    if (existingDb) {
        std::ofstream report((db.path().parent_path() / "money-migration-report.txt").string());
        report << "Money migration: columns renamed to rupee names. Stored values are exact\n"
               << "hundredths of a rupee and were NOT changed. Before vs after totals:\n\n";
        bool okAll = true;
        for (int i = 0; i < checks.size(); ++i) {
            const auto after = sumColumn(db, checks[i].table, checks[i].newCol);
            const bool ok = after == before[i];
            okAll = okAll && ok;
            report << checks[i].label << ": before PKR " << formatMoney(before[i]).toStdString()
                   << "  after PKR " << formatMoney(after).toStdString() << (ok ? "  OK\n" : "  *** MISMATCH ***\n");
        }
        report << (okAll ? "\nAll totals match.\n" : "\nMISMATCH DETECTED.\n");
        report.close();
        if (!okAll) throw DatabaseError("money migration verification failed: totals changed during column rename");
    }
}

void addMissingColumns(Database& db) {
    const QList<QPair<QString,QStringList>> additions={{"customers",{"phone TEXT","address TEXT","credit_limit INTEGER NOT NULL DEFAULT 0","payment_terms_days INTEGER NOT NULL DEFAULT 0","balance INTEGER NOT NULL DEFAULT 0","is_deleted INTEGER NOT NULL DEFAULT 0"}},{"sales",{"warehouse_id TEXT REFERENCES warehouses(id)","shift_id TEXT REFERENCES shift_sessions(id)","discount INTEGER NOT NULL DEFAULT 0","tax INTEGER NOT NULL DEFAULT 0","paid INTEGER NOT NULL DEFAULT 0","due INTEGER NOT NULL DEFAULT 0","note TEXT","tendered INTEGER NOT NULL DEFAULT 0","voided_at TEXT","void_reason TEXT"}},{"sale_items",{"batch_id TEXT","unit_name TEXT NOT NULL DEFAULT 'base'","unit_price INTEGER NOT NULL DEFAULT 0","discount INTEGER NOT NULL DEFAULT 0","line_total INTEGER NOT NULL DEFAULT 0","course_id TEXT","total_pct_bp INTEGER NOT NULL DEFAULT 0","partner_pct_bp INTEGER NOT NULL DEFAULT 0","partner_id TEXT"}},{"bundles",{"total_pct_bp INTEGER NOT NULL DEFAULT 0","partner_pct_bp INTEGER NOT NULL DEFAULT 0","partner_id TEXT"}},{"suppliers",{"contact_person TEXT","phone TEXT","address TEXT","opening_balance INTEGER NOT NULL DEFAULT 0","balance INTEGER NOT NULL DEFAULT 0","is_archived INTEGER NOT NULL DEFAULT 0"}},{"products",{"sku TEXT","barcode TEXT","category_id TEXT","brand_id TEXT","description TEXT NOT NULL DEFAULT ''","track_batches INTEGER NOT NULL DEFAULT 0","track_expiry INTEGER NOT NULL DEFAULT 0","purchase_price INTEGER NOT NULL DEFAULT 0","retail_price INTEGER NOT NULL DEFAULT 0","wholesale_price INTEGER NOT NULL DEFAULT 0","dealer_price INTEGER NOT NULL DEFAULT 0","stock_quantity INTEGER NOT NULL DEFAULT 0","minimum_stock INTEGER NOT NULL DEFAULT 0","image_path TEXT","is_deleted INTEGER NOT NULL DEFAULT 0","total_pct_bp INTEGER NOT NULL DEFAULT 0","partner_pct_bp INTEGER NOT NULL DEFAULT 0","partner_id TEXT"}},{"purchases",{"warehouse_id TEXT","subtotal INTEGER NOT NULL DEFAULT 0","discount INTEGER NOT NULL DEFAULT 0","tax INTEGER NOT NULL DEFAULT 0","paid INTEGER NOT NULL DEFAULT 0","due INTEGER NOT NULL DEFAULT 0","notes TEXT"}},{"cash_transactions",{"drawer_id TEXT","shift_id TEXT","sale_id TEXT","reason TEXT"}},{"cheques",{"party_id TEXT","bank TEXT"}},{"customer_payments",{"note TEXT"}},{"customer_ledger",{"sale_id TEXT"}},{"batches",{"expiry_date TEXT"}}};
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
)SQL",
R"SQL(
CREATE TABLE IF NOT EXISTS sale_payments (id TEXT PRIMARY KEY, sale_id TEXT NOT NULL REFERENCES sales(id), method TEXT NOT NULL, amount_paisa INTEGER NOT NULL CHECK(amount_paisa>0), created_at TEXT NOT NULL);
CREATE INDEX IF NOT EXISTS sale_payments_sale ON sale_payments(sale_id);
)SQL",
R"SQL(
CREATE TABLE bundles (id TEXT PRIMARY KEY, name TEXT NOT NULL, grade_label TEXT NOT NULL DEFAULT '', description TEXT NOT NULL DEFAULT '', is_archived INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL);
CREATE TABLE bundle_items (id TEXT PRIMARY KEY, bundle_id TEXT NOT NULL REFERENCES bundles(id), product_id TEXT NOT NULL REFERENCES products(id), quantity INTEGER NOT NULL CHECK(quantity>0), sort_order INTEGER NOT NULL DEFAULT 0, UNIQUE(bundle_id,product_id));
CREATE INDEX bundle_items_bundle ON bundle_items(bundle_id, sort_order);
CREATE TABLE sale_item_commissions (id TEXT PRIMARY KEY, sale_item_id TEXT NOT NULL REFERENCES sale_items(id), sale_id TEXT NOT NULL REFERENCES sales(id), product_id TEXT NOT NULL REFERENCES products(id), retail_amount_paisa INTEGER NOT NULL, commission_rate_bp INTEGER NOT NULL, commission_amount_paisa INTEGER NOT NULL, partner_rate_bp INTEGER NOT NULL, partner_amount_paisa INTEGER NOT NULL, discount_amount_paisa INTEGER NOT NULL, owner_amount_paisa INTEGER NOT NULL, overridden INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL);
CREATE INDEX sale_item_commissions_sale ON sale_item_commissions(sale_id, created_at);
CREATE INDEX sale_item_commissions_product ON sale_item_commissions(product_id, created_at);
)SQL",
R"SQL(
CREATE TABLE partners (id TEXT PRIMARY KEY, name TEXT NOT NULL, phone TEXT, notes TEXT NOT NULL DEFAULT '', balance_paisa INTEGER NOT NULL DEFAULT 0, is_archived INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL);
CREATE TABLE partner_ledger (id TEXT PRIMARY KEY, partner_id TEXT NOT NULL REFERENCES partners(id), entry_type TEXT NOT NULL, sale_id TEXT REFERENCES sales(id), source_type TEXT, source_id TEXT, description TEXT NOT NULL, debit_paisa INTEGER NOT NULL DEFAULT 0, credit_paisa INTEGER NOT NULL DEFAULT 0, running_balance_paisa INTEGER NOT NULL, created_at TEXT NOT NULL);
CREATE INDEX partner_ledger_lookup ON partner_ledger(partner_id, created_at);
CREATE TABLE profit_entries (id TEXT PRIMARY KEY, sale_id TEXT REFERENCES sales(id), source_type TEXT NOT NULL, source_id TEXT, partner_id TEXT REFERENCES partners(id), description TEXT NOT NULL, sale_value_paisa INTEGER NOT NULL, commission_paisa INTEGER NOT NULL, partner_amount_paisa INTEGER NOT NULL, my_profit_paisa INTEGER NOT NULL, total_pct_bp INTEGER NOT NULL, partner_pct_bp INTEGER NOT NULL, is_reversal INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL);
CREATE INDEX profit_entries_sale ON profit_entries(sale_id);
CREATE INDEX profit_entries_source ON profit_entries(source_type, source_id, created_at);
CREATE INDEX profit_entries_partner ON profit_entries(partner_id, created_at);
ALTER TABLE bundles ADD COLUMN total_pct_bp INTEGER NOT NULL DEFAULT 0;
ALTER TABLE bundles ADD COLUMN partner_pct_bp INTEGER NOT NULL DEFAULT 0;
ALTER TABLE bundles ADD COLUMN partner_id TEXT REFERENCES partners(id);
ALTER TABLE products ADD COLUMN total_pct_bp INTEGER NOT NULL DEFAULT 0;
ALTER TABLE products ADD COLUMN partner_pct_bp INTEGER NOT NULL DEFAULT 0;
ALTER TABLE products ADD COLUMN partner_id TEXT REFERENCES partners(id);
ALTER TABLE sale_items ADD COLUMN course_id TEXT;
ALTER TABLE sale_items ADD COLUMN total_pct_bp INTEGER NOT NULL DEFAULT 0;
ALTER TABLE sale_items ADD COLUMN partner_pct_bp INTEGER NOT NULL DEFAULT 0;
ALTER TABLE sale_items ADD COLUMN partner_id TEXT;
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
    // Rename *_paisa money columns to rupee names (values unchanged). Runs after the
    // versioned migrations so every money column exists, and BEFORE addMissingColumns
    // so that helper (now using rupee names) doesn't re-add legacy paisa columns.
    migrateMoneyColumnsToRupees(db, version > 0);
    Transaction compatibility(db.handle());
    addMissingColumns(db);
    db.exec("CREATE INDEX IF NOT EXISTS customers_phone_lookup ON customers(phone); CREATE INDEX IF NOT EXISTS cheques_due_status ON cheques(due_date,status);");
    compatibility.commit();
}
} // namespace pos
