# Backlog

## Highest priority

- [x] Complete schema compatibility migration and old-database upgrade test.
- [x] Wire active shift IDs into cash sales, purchases, and customer payments; add automatic active-till attachment.
- [x] Add Cash Management open/close UI and explicit till status indicator.
- [x] Implement sales and purchase returns/refunds with atomic stock/ledger/cash reversals.
- [x] Replace placeholder screens: Suppliers, Cheques, Reports, and Audit Log.
- [x] Replace placeholder screen: Settings (offline business profile and receipt settings persistence).
- [x] Replace Analytics placeholder with an offline KPI and seven-day sales dashboard.

## Next

- [x] Add supplier ledger screen; customer payment allocation UI is available from Customers.
- [x] Add report CSV and PDF export.
- [x] Add validated atomic product CSV import.
- [x] Add native offline `.xlsx` report output.
- [x] Add A4 report printing through the local Qt print dialog.
- [x] Add offline ESC/POS thermal receipt and Code128 barcode-label output with configured raw-device and test controls.
- [x] Add scanner return-key timing and barcode auto-add behavior in Sales POS.
- [x] Add suspended-sale persistence service with JSON validation and rollback-safe storage.
- [x] Add Sales POS suspend/resume controls using SuspendedSaleService.
- [x] Add invoice discount capture and validation in Sales POS.
- [x] Add split-tender payment persistence and mixed-payment capture.
- [x] Add salted PIN setup/verification in local settings and enforce it on shift close, supplier archive, backup restore, and PIN removal.
- [x] Add notification persistence, unread retrieval, and read-state service.
- [x] Add notifications UI with unread empty-state handling and read-on-view behavior in Settings.
- [x] Generate best-effort operational notifications for completed sales, verified backups, shift lifecycle events, and low-stock sales.
- [x] Add Inventory edit and archive controls backed by InventoryService.
- [x] Backup UI lets operators choose a local or USB destination and restore from a selected external `.db` file with integrity/PIN checks.
- [x] Add retention pruning for verified backups (default UI policy keeps the newest 30).
- [x] Add persisted automatic backup interval settings and an offline QTimer scheduler (restart applies changes).
- [ ] [P1][blocked] Define and implement cheque reversal accounting (link cheque rows to the originating sale/purchase/payment and reverse the corresponding ledger/cash effect); dependency: business rule for bounced/cleared cheque settlement.

## Quality and release

- [x] Add migration, shift, return, thermal, seed, backup retention, notification event, cheque, report, audit, inventory service, and multi-step rollback regression coverage.
- [x] Add Qt UI smoke coverage for Inventory/Settings/Backup through the `pos_ui_smoke_tests` CTest target.
- [ ] Add interactive Windows cursor/keyboard E2E coverage; dependency: a desktop test runner/session that exposes a stable application window handle.
- [ ] [P2][blocked] Add a sanitizer-enabled test job; dependency: compiler/Qt toolchain that ships ASan/UBSan runtime libraries (the bundled MinGW toolchain does not).
- [x] Add deterministic, idempotent demo and randomized QA seed data via `wholesale_pos.exe --seed-demo` and `--seed-random=<count> --seed=<seed>`.
- [x] Add Windows deployment staging automation and synchronize the support/recovery runbook.
- [x] Add a step-by-step user guide covering setup, operations, backups, printing, keyboard use, and QA data.
- [ ] Add visible Sales Return and Purchase Return UI workflows backed by `ReturnService`.
- [ ] [P1][blocked] Evaluate SQLCipher and offline licensing separately; dependencies: SQLCipher distribution/license choice and product licensing policy.
