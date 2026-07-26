# Nexora POS Project

Nexora POS is an offline-first Windows wholesale POS built with C++20, Qt 6 Widgets, CMake, and vendored SQLite. The application stores business data in a per-user SQLite database with WAL mode and uses UUID primary keys and integer paisa money values.

## Architecture

- `src/core`: UI-independent database, migrations, transactional services, backup, reporting, audit, settings, inventory, POS, purchasing, payments, shifts, cheques, and catalog services.
- `src/ui`: Qt Widgets shell, QSS themes, live Inventory, Sales POS, Purchases, Customers, Suppliers, Cash Management, Cheques, Reports, Analytics, Audit Log, Settings, and Backup/Restore pages.
- `tests`: QtTest business-logic coverage.
- `third_party/sqlite`: vendored SQLite amalgamation.

The intended dependency direction is UI → services → Database/SQLite. Services own transaction boundaries; the UI must not perform business writes directly.

Schema compatibility is checked at startup. The migration runner repairs missing legacy columns using inspected, parameterized-safe static definitions and creates compatibility indexes after repair.

## Current implemented workflows

Product CRUD, inventory receipts/adjustments, FEFO multi-batch sales, atomic sales and purchase returns, cash sale voids, shift-attached cash sales/purchases/payments, purchase receiving, customer payments with invoice allocation UI, supplier CRUD with ledger display, shift lifecycle and reconciliation UI, cheque register, summary reporting with CSV/PDF export, audit querying, persisted business settings, verified online backup/restore, and barcode validation.

## Known limitations

Complete cheque reversal behavior, printing, exports, settings PIN protection, notifications, scheduled backups, SQLCipher, and licensing remain backlog work. FBR is intentionally excluded from the product scope.
