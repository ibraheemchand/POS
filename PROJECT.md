# Nexora POS Project

Nexora POS is an offline-first Windows wholesale POS built with C++20, Qt 6 Widgets, CMake, and vendored SQLite. The application stores business data in a per-user SQLite database with WAL mode and uses UUID primary keys and integer paisa money values.

## Architecture

- `src/core`: UI-independent database, migrations, transactional services, backup, reporting, audit, settings, security, suspended-sale persistence, notifications, inventory, POS, purchasing, payments, shifts, cheques, Excel export, seed data, thermal printing, and catalog services.
- `src/ui`: Qt Widgets shell, QSS themes, live Inventory, Sales POS, Purchases, Customers, Suppliers, Cash Management, Cheques, Reports, Analytics, Audit Log, Settings, and Backup/Restore pages.
- `tests`: QtTest business-logic coverage.
- `third_party/sqlite`: vendored SQLite amalgamation.
- `scripts/deploy.ps1` stages a clean Windows deployment with `windeployqt`; `installer/NexoraPOS.iss` packages the staged directory.
- `wholesale_pos.exe --seed-demo` inserts deterministic demo products, stock, supplier, and customer records once for QA/support environments.

The intended dependency direction is UI → services → Database/SQLite. Services own transaction boundaries; the UI must not perform business writes directly.

Schema compatibility is checked at startup. The migration runner repairs missing legacy columns using inspected, parameterized-safe static definitions and creates compatibility indexes after repair.

## Current implemented workflows

Product CRUD with atomic CSV import, inventory edit/archive controls, scanner return-key barcode auto-add, offline ESC/POS thermal receipts and Code128 barcode labels, suspend/resume sales, invoice discounts, mixed tender persistence/capture, inventory receipts/adjustments, FEFO multi-batch sales, atomic sales and purchase returns, cash sale voids, shift-attached cash sales/purchases/payments, purchase receiving, customer payments with invoice allocation UI, supplier CRUD with ledger display, shift lifecycle and reconciliation UI, cheque register, summary reporting with CSV/PDF/XLSX export and A4 printing, audit querying, persisted business settings, verified online backup/restore, unread-notification viewing, and barcode validation.

## Known limitations

Complete cheque reversal behavior, SQLCipher, and licensing remain backlog work. ESC/POS output writes validated raw receipt/Code128 bytes to the configured local device path. Settings persist an optional automatic backup interval (applied on restart), and verified backup retention pruning keeps the newest 30 snapshots by default. Backup UI supports choosing local/USB destinations and external restore files. Operational notifications are generated best-effort after sales, backups, and shift events. FBR is intentionally excluded from the product scope.
