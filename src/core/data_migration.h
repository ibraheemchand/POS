#pragma once
#include <QString>

namespace pos {

// One-time migration of a database from the old per-user location into the new shared
// %PROGRAMDATA%\Invento location. The old file is COPIED, never moved, and the copy
// is verified (integrity + row counts + money totals) before the app uses it. If
// anything differs, the app keeps using the old database and reports an error.
struct MigrationOutcome {
    enum Status { Fresh, AlreadyPresent, Migrated, Failed };
    Status status{Fresh};
    QString databaseDir;  // directory the app should actually open
    QString message;      // human-readable info (or error) to show; empty if nothing to say
    bool isError{false};
};

MigrationOutcome migrateLegacyDatabaseIfNeeded();

// Read-only comparison of two database files: PRAGMA integrity_check on the new file,
// equal row counts for every table, and equal money totals for sales/purchases.
// Never modifies either file. Exposed for testing the migration safety check.
bool verifyDatabasesMatch(const QString& oldPath, const QString& newPath, QString* error);

} // namespace pos
