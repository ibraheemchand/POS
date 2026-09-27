#pragma once
#include <QString>

// Central, single source of truth for where Invento keeps its data. The app
// NEVER writes inside its install folder (Program Files is read-only for users).
// Shared data lives under %PROGRAMDATA%\Invento so every Windows user on the
// terminal sees the same sales/inventory/cash.
namespace pos::paths {

QString appDataDir();       // %PROGRAMDATA%\Invento (created)
QString databaseFilePath(); // <appDataDir>\business.db
QString backupsDir();       // <appDataDir>\backups (created)
QString logsDir();          // <appDataDir>\logs (created)
QString receiptsDir();      // Documents\Invento Receipts (created) — user-facing PDFs

// Old locations from the "Invento" builds, checked once for one-time migration.
QString legacyAppDataDir(); // %APPDATA%\Invento\Invento
QString legacyExeDir();     // folder next to the running exe

} // namespace pos::paths
