#include "core/data_migration.h"
#include "core/app_paths.h"
#include "sqlite3.h"
#include <QFileInfo>
#include <QDir>
#include <filesystem>

namespace pos {
namespace {

// A read-only sqlite handle. Crucially this never runs migrations, so the OLD
// database file is inspected without being modified in any way.
struct ReadOnlyDb {
    sqlite3* db{nullptr};
    explicit ReadOnlyDb(const QString& path) {
        if (sqlite3_open_v2(path.toUtf8().constData(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            if (db) { sqlite3_close(db); db = nullptr; }
        }
    }
    ~ReadOnlyDb() { if (db) sqlite3_close(db); }
    bool ok() const { return db != nullptr; }
};

qint64 scalarInt(sqlite3* db, const QString& sql, bool* ok) {
    *ok = false;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.toUtf8().constData(), -1, &stmt, nullptr) != SQLITE_OK) return 0;
    qint64 value = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) { value = sqlite3_column_int64(stmt, 0); *ok = true; }
    sqlite3_finalize(stmt);
    return value;
}

QString scalarText(sqlite3* db, const QString& sql) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.toUtf8().constData(), -1, &stmt, nullptr) != SQLITE_OK) return {};
    QString value;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (text) value = QString::fromUtf8(text);
    }
    sqlite3_finalize(stmt);
    return value;
}

QStringList tableNames(sqlite3* db) {
    QStringList names;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name", -1, &stmt, nullptr) != SQLITE_OK) return names;
    while (sqlite3_step(stmt) == SQLITE_ROW) names.append(QString::fromUtf8(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))));
    sqlite3_finalize(stmt);
    return names;
}

// Sum of a table's money column, tolerant of the old paisa-suffixed schema.
qint64 moneyTotal(sqlite3* db, const QString& table, bool* ok) {
    for (const QString& col : {QString("total"), QString("total_paisa")}) {
        qint64 v = scalarInt(db, QString("SELECT COALESCE(SUM(%1),0) FROM %2").arg(col, table), ok);
        if (*ok) return v;
    }
    *ok = false;
    return 0;
}

} // namespace

bool verifyDatabasesMatch(const QString& oldPath, const QString& newPath, QString* err) {
    ReadOnlyDb oldDb(oldPath), newDb(newPath);
    if (!oldDb.ok() || !newDb.ok()) { *err = "one of the databases could not be opened for verification"; return false; }

    if (scalarText(newDb.db, "PRAGMA integrity_check") != "ok") { *err = "the copied database failed PRAGMA integrity_check"; return false; }

    for (const auto& table : tableNames(oldDb.db)) {
        bool okOld = false, okNew = false;
        const auto c1 = scalarInt(oldDb.db, "SELECT COUNT(*) FROM " + table, &okOld);
        const auto c2 = scalarInt(newDb.db, "SELECT COUNT(*) FROM " + table, &okNew);
        if (!okOld || !okNew || c1 != c2) { *err = QString("row count differs for table \"%1\" (old %2, new %3)").arg(table).arg(c1).arg(c2); return false; }
    }

    for (const QString& table : {QString("sales"), QString("purchases")}) {
        bool okOld = false, okNew = false;
        const auto t1 = moneyTotal(oldDb.db, table, &okOld);
        const auto t2 = moneyTotal(newDb.db, table, &okNew);
        if (okOld != okNew || (okOld && t1 != t2)) { *err = QString("money total differs for \"%1\" (old %2, new %3)").arg(table).arg(t1).arg(t2); return false; }
    }
    return true;
}

static QString firstLegacyDatabase() {
    for (const QString& dir : {paths::legacyExeDir(), paths::legacyAppDataDir()}) {
        if (dir.isEmpty()) continue;
        const QString candidate = QDir(dir).filePath("business.db");
        if (QFileInfo::exists(candidate)) return candidate;
    }
    return {};
}

MigrationOutcome migrateLegacyDatabaseIfNeeded() {
    MigrationOutcome out;
    const QString newDir = paths::appDataDir();
    const QString newDb = paths::databaseFilePath();
    out.databaseDir = newDir;

    if (QFileInfo::exists(newDb)) { out.status = MigrationOutcome::AlreadyPresent; return out; }

    const QString legacyDb = firstLegacyDatabase();
    if (legacyDb.isEmpty()) { out.status = MigrationOutcome::Fresh; return out; }
    const QString legacyDir = QFileInfo(legacyDb).absolutePath();

    std::error_code ec;
    std::filesystem::copy_file(legacyDb.toStdWString(), newDb.toStdWString(), std::filesystem::copy_options::overwrite_existing, ec);
    // Also bring any WAL/SHM sidecars so no committed-but-not-checkpointed data is lost.
    for (const QString& suffix : {QString("-wal"), QString("-shm")}) {
        std::error_code sec;
        const QString src = legacyDb + suffix, dst = newDb + suffix;
        if (QFileInfo::exists(src))
            std::filesystem::copy_file(src.toStdWString(), dst.toStdWString(), std::filesystem::copy_options::overwrite_existing, sec);
    }
    if (ec) {
        std::filesystem::remove(newDb.toStdWString(), ec);
        out.status = MigrationOutcome::Failed;
        out.isError = true;
        out.databaseDir = legacyDir; // keep using the old database
        out.message = QString("Could not copy your existing database from:\n%1\n\nThe app will keep using the old database. The old folder was left untouched.").arg(legacyDir);
        return out;
    }

    QString err;
    if (!verifyDatabasesMatch(legacyDb, newDb, &err)) {
        std::filesystem::remove(newDb.toStdWString(), ec); // discard the unverified copy
        out.status = MigrationOutcome::Failed;
        out.isError = true;
        out.databaseDir = legacyDir; // keep using the old database
        out.message = QString("Database migration was aborted for safety: %1.\n\nThe app will keep using your old database at:\n%2\n\nThe old folder was left untouched.").arg(err, legacyDir);
        return out;
    }

    out.status = MigrationOutcome::Migrated;
    out.message = QString("Copied your existing database to the new shared location:\n%1\n\nThe original at %2 was left untouched (nothing was moved or deleted).").arg(newDir, legacyDir);
    return out;
}

} // namespace pos
