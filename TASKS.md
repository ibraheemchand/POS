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
- [ ] Add ESC/POS thermal and barcode-label printing.
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

## Quality and release

- [ ] Add migration, shift, return, inventory UI, and sanitizer tests; backup retention, notification events, cheque, report, audit, and inventory service coverage is now present.
- [ ] Add seed data, deployment automation, and support-runbook updates.
- [ ] Evaluate SQLCipher and offline licensing separately; neither is enabled today.
# Legacy Sprint List

This file is retained for historical context. The active backlog is `TASKS.md`.

## High Priority
- [ ] Product Management (CRUD)
- [ ] Customer Management
- [ ] Supplier Management
- [ ] Purchase Entry
- [ ] Sales Screen
- [ ] Receipt Printing (ESC/POS)
- [ ] Inventory Adjustments
- [ ] Cash In / Cash Out
- [ ] Customer Ledger
- [ ] Supplier Ledger

## Medium Priority
- [ ] Reports
- [ ] Analytics Dashboard
- [ ] Barcode Support
- [ ] Backup & Restore
- [ ] Settings
- [ ] Database Maintenance

## Polish
- [ ] Keyboard Shortcuts
- [ ] Error Handling
- [ ] Performance Optimization
- [ ] UI Improvements
