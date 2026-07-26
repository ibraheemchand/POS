#include "core/database.h"
#include "core/migrations.h"
#include <QUuid>
#include <QDateTime>
#include <QFile>

namespace pos {
namespace {
void check(int code, sqlite3* db, const char* context) {
    if (code != SQLITE_OK && code != SQLITE_DONE && code != SQLITE_ROW)
        throw DatabaseError(QString("%1: %2").arg(context, sqlite3_errmsg(db)).toStdString());
}
}

Statement::Statement(sqlite3* db, const char* sql) {
    check(sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr), db, "prepare");
}
Statement::~Statement() { if (stmt_) sqlite3_finalize(stmt_); }
void Statement::bind(int i, const QString& v) { check(sqlite3_bind_text(stmt_, i, v.toUtf8().constData(), -1, SQLITE_TRANSIENT), sqlite3_db_handle(stmt_), "bind text"); }
void Statement::bind(int i, qint64 v) { check(sqlite3_bind_int64(stmt_, i, v), sqlite3_db_handle(stmt_), "bind int"); }
void Statement::bindNull(int i) { check(sqlite3_bind_null(stmt_, i), sqlite3_db_handle(stmt_), "bind null"); }
bool Statement::stepRow() { const int r=sqlite3_step(stmt_); check(r, sqlite3_db_handle(stmt_), "step"); return r==SQLITE_ROW; }
void Statement::execute() { if (stepRow()) throw DatabaseError("statement unexpectedly returned a row"); }
QString Statement::text(int c) const { const auto* v=sqlite3_column_text(stmt_, c); return v ? QString::fromUtf8(reinterpret_cast<const char*>(v)) : QString(); }
qint64 Statement::integer(int c) const { return sqlite3_column_int64(stmt_, c); }

Transaction::Transaction(sqlite3* db) : db_(db) { check(sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr), db_, "begin transaction"); }
Transaction::~Transaction() { if (!complete_) sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr); }
void Transaction::commit() { check(sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr), db_, "commit transaction"); complete_=true; }

Database::Database(const std::filesystem::path& path) : path_(path) {
    const auto utf8 = path.u8string();
    check(sqlite3_open_v2(reinterpret_cast<const char*>(utf8.c_str()), &db_, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX, nullptr), db_, "open database");
    exec("PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA busy_timeout=5000; PRAGMA synchronous=NORMAL;");
}
Database::~Database() { if (db_) sqlite3_close_v2(db_); }
void Database::exec(const char* sql) const { char* error=nullptr; const int r=sqlite3_exec(db_, sql, nullptr, nullptr, &error); if(r!=SQLITE_OK) { const QString message=error?QString::fromUtf8(error):QString::fromUtf8(sqlite3_errmsg(db_)); sqlite3_free(error); throw DatabaseError(message.toStdString()); } }
Statement Database::prepare(const char* sql) const { return Statement(db_, sql); }
void Database::migrate() { applyMigrations(*this); }
bool Database::quickCheck() const { auto stmt=prepare("PRAGMA quick_check"); return stmt.stepRow() && stmt.text(0)=="ok"; }
bool Database::integrityCheck() const { auto stmt=prepare("PRAGMA integrity_check"); return stmt.stepRow() && stmt.text(0)=="ok"; }
bool Database::isSchemaCompatible(QStringList* missing) const {
    const QList<QPair<QString,QStringList>> required={{"customers",{"id","name","phone","address","credit_limit_paisa","payment_terms_days","balance_paisa","created_at","is_deleted"}},{"sales",{"id","invoice_no","customer_id","warehouse_id","shift_id","status","payment_method","subtotal_paisa","discount_paisa","tax_paisa","total_paisa","paid_paisa","due_paisa","note","created_at","voided_at","void_reason"}},{"sale_items",{"id","sale_id","product_id","batch_id","quantity","unit_name","unit_price_paisa","discount_paisa","line_total_paisa"}},{"suppliers",{"id","name","contact_person","phone","address","opening_balance_paisa","balance_paisa","is_archived","created_at"}},{"products",{"id","name","sku","barcode","category_id","brand_id","description","base_unit","track_batches","track_expiry","purchase_price_paisa","retail_price_paisa","wholesale_price_paisa","dealer_price_paisa","stock_quantity","minimum_stock","image_path","is_deleted","created_at","updated_at"}},{"purchases",{"id","invoice_no","supplier_id","warehouse_id","status","subtotal_paisa","discount_paisa","tax_paisa","total_paisa","paid_paisa","due_paisa","purchased_at","notes"}},{"cash_transactions",{"id","drawer_id","shift_id","sale_id","type","amount_paisa","reason","created_at"}},{"cheques",{"id","direction","party_id","cheque_no","bank","amount_paisa","due_date","status","created_at"}},{"customer_payments",{"id","customer_id","method","amount_paisa","note","created_at"}},{"customer_ledger",{"id","customer_id","sale_id","description","debit_paisa","credit_paisa","running_balance_paisa","created_at"}},{"batches",{"id","product_id","batch_no","expiry_date","quantity_remaining","purchase_price_paisa","created_at"}}};
    QStringList absent; for(const auto& table:required){QStringList actual;auto q=prepare(("PRAGMA table_info("+table.first+")").toUtf8().constData());while(q.stepRow())actual.append(q.text(1));for(const auto& column:table.second)if(!actual.contains(column))absent.append(table.first+"."+column);} if(missing)*missing=absent;return absent.isEmpty();
}
void Database::backupTo(const std::filesystem::path& destination) const {
    const int checkpoint=sqlite3_wal_checkpoint_v2(db_, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);
    if (checkpoint!=SQLITE_OK && checkpoint!=SQLITE_BUSY) check(checkpoint, db_, "checkpoint WAL");
    sqlite3* target{}; const auto p=destination.u8string();
    check(sqlite3_open_v2(reinterpret_cast<const char*>(p.c_str()), &target, SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, nullptr), target, "open backup");
    sqlite3_backup* backup=sqlite3_backup_init(target, "main", db_, "main");
    if (!backup) { sqlite3_close(target); throw DatabaseError("cannot initialize online backup"); }
    const int result=sqlite3_backup_step(backup, -1); sqlite3_backup_finish(backup);
    check(result==SQLITE_DONE ? SQLITE_OK : result, target, "backup"); sqlite3_close(target);
}
void Database::restoreFrom(const std::filesystem::path& source) {
    sqlite3* snapshot{}; const auto p=source.u8string();
    check(sqlite3_open_v2(reinterpret_cast<const char*>(p.c_str()), &snapshot, SQLITE_OPEN_READONLY, nullptr), snapshot, "open restore snapshot");
    sqlite3_backup* restore=sqlite3_backup_init(db_, "main", snapshot, "main");
    if (!restore) { sqlite3_close(snapshot); throw DatabaseError("cannot initialize restore"); }
    const int result=sqlite3_backup_step(restore, -1); sqlite3_backup_finish(restore);
    check(result==SQLITE_DONE ? SQLITE_OK : result, db_, "restore database"); sqlite3_close(snapshot);
}
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString utcNow() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
} // namespace pos
