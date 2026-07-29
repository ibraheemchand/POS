# Nexora POS

An offline-first Windows wholesale POS foundation written in C++20, Qt 6 Widgets, and SQLite. It starts with a modern business shell and the safety-critical data path: integer-money accounting, UUIDs, WAL-mode SQLite, migrations, atomic sale/stock/ledger writes, FEFO lot selection, void audit logging, and verified online backups.

## Current scope

This repository is an intentionally buildable offline POS application. It includes a Qt desktop shell, transaction-critical workflows, verified backup/restore, scheduled backups, local reporting, ESC/POS output, and barcode labels. FBR transport remains intentionally excluded.

Implemented core guarantees:

- `Money` is `qint64` paisa everywhere in the business core.
- Every sale and void uses one RAII SQLite transaction; any failure rolls back all touched stock, ledger, cash, and audit changes.
- Database connections enable foreign keys, WAL, and a five-second busy timeout.
- Products, ledger entries, transactions, and operational entities use UUID primary keys.
- Batch-tracked sales choose the first qualifying non-expired lot by FEFO ordering. Inventory is changed only through stock movements.
- A sale can consume multiple FEFO batches in one atomic operation; line discounts are apportioned without losing paisa.
- Sale voids return stock and post compensating cash/ledger entries. Sales with allocated customer payments are deliberately blocked from voiding and must follow a refund path.
- Purchases atomically receive stock/batches, write purchase history and stock movements, update supplier payables, and record cash outflows.
- Customer payments allocate to one or more open invoices, update each invoice balance, customer ledger, and cash book together.
- Backups use SQLite's online backup API, checkpoint WAL, run `integrity_check`, and create a SHA-256 sidecar checksum. The Backup & Restore screen verifies a selected snapshot, takes a safety backup of the live database, restores through SQLite's backup API, then integrity-checks the result.
- Startup checks the live database and stops safely if it is unhealthy.

## Build

Prerequisites: CMake 3.24+, a C++20 compiler, and Qt 6.4+ (Widgets, Sql, Test). SQLite 3.53.4 is vendored under `third_party/sqlite`, so no database package or internet download is required.

```powershell
cmake -S . -B build -DPOS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Run `build\Release\wholesale_pos.exe` (or the platform-specific build output). For deployment on Windows, run Qt's `windeployqt` on the executable and package the deployed directory with Inno Setup or NSIS. The data file deliberately lives in Qt's per-user app-data location, not beside the binary, so normal upgrades preserve business data.

For QA/support fixtures, run `wholesale_pos.exe --seed-demo` once against the target per-user data directory. To generate priced random fixtures, run `wholesale_pos.exe --data-dir=<temporary-folder> --seed-random=25 --seed=20260727`. Both modes are explicit and idempotent; normal application startup never inserts data.

See the complete [step-by-step user guide](docs/USER_GUIDE.md) for setup, sales, purchasing, shifts, reports, backups, printing, and QA data.

## Architecture

```
src/core/       UI-independent business and SQLite layers
src/ui/         Qt Widgets shell and QSS theme
tests/          QtTest coverage for business transactions and the MainWindow UI smoke target
```

The remaining decisions are cheque reversal accounting rules, a sanitizer-capable compiler runtime, and an explicit SQLCipher/licensing choice. Demo/random seeding, deployment staging, thermal output, and UI smoke coverage are delivered. Keep FBR behind `POS_ENABLE_FBR`; it must never become a dependency of sale completion.

## Operational notes

- Default money unit is paisa. Do not introduce `double` fields or calculations for prices, tax, discounts, or balances.
- The current sale service hard-blocks credit-limit violations. Expose the configured warn/block policy only when its audit behavior is implemented.
- The included `fbr_queue` table is a dormant schema seam. No network code is linked unless the optional module is explicitly built.
- SQLCipher is not enabled in this starter because it requires a SQLCipher-linked SQLite build. Replace `SQLite::SQLite3` with your SQLCipher target at packaging time and apply the key before migrations.
