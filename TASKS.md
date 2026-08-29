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

## UI redesign and refactor

- [x] Extract Stitch design tokens from `docs/design-reference/` and apply them to the Qt QSS theme.
- [x] Rebuild Dashboard layout to match `kinetic_inventory_dashboard` mockup structure and density.
- [x] Rebuild Inventory layout to match `kinetic_inventory_stock_list` mockup structure and density.
- [x] Apply Stitch tokens and layout patterns to Sales POS, Purchases, Customers, Suppliers, Cash & Shifts, and Reports pages.
- [x] Remove the standalone Analytics page; fold any unique KPI content into Dashboard or Reports.
- [x] Move `stitch/` and `mainMenuSTITCH/` into `docs/design-reference/` and document them in `PROJECT.md`.
- [x] Add bundled SVG action icons to `src/ui/assets/` and register them in `resources.qrc`.
- [x] Replace all in-app `QApplication::style()->standardIcon` usage with bundled `QIcon(":/icons/...")` resources.
- [x] Extract `makeDashboard()` into `src/ui/pages/dashboard_page.{h,cpp}`.
- [x] Extract `makeInventory()` into `src/ui/pages/inventory_page.{h,cpp}`.
- [x] Extract `makeSalesPos()` into `src/ui/pages/sales_pos_page.{h,cpp}`.
- [x] Extract `makePurchases()` into `src/ui/pages/purchases_page.{h,cpp}`.
- [x] Extract `makeCustomers()` into `src/ui/pages/customers_page.{h,cpp}`.
- [x] Extract `makeSuppliers()` into `src/ui/pages/suppliers_page.{h,cpp}`.
- [x] Extract `makeCashManagement()` into `src/ui/pages/cash_management_page.{h,cpp}`.
- [x] Extract `makeCheques()` into `src/ui/pages/cheques_page.{h,cpp}`.
- [x] Extract `makeReports()` into `src/ui/pages/reports_page.{h,cpp}`.
- [x] Extract `makeAuditLog()` into `src/ui/pages/audit_log_page.{h,cpp}`.
- [x] Extract `makeSettings()` into `src/ui/pages/settings_page.{h,cpp}`.
- [x] Extract `makeBackupRestore()` into `src/ui/pages/backup_restore_page.{h,cpp}`.
- [x] Reduce `main_window.cpp` to navigation shell, shortcut bar, and `DataChangeBus` wiring only.
- [x] Add service read methods for UI table population; remove raw `database_->prepare` from UI page classes.
- [x] Add Alt-mnemonics and explicit `setTabOrder()` on every primary page form.
- [x] Make the shortcut hint bar fully dynamic per active page.
- [x] Add arrow-key / Enter / Delete keyboard navigation to Customers, Suppliers, and Cheques table pages.
- [x] Verify a full sale can be completed keyboard-only after the refactor.
