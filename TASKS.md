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
- [ ] Add CSV import and Excel report output.
- [ ] Add ESC/POS/A4/barcode-label printing.
- [ ] Add scanner timing, suspend/resume sales, discounts, mixed payments, and sensitive-action PIN.
- [ ] Add notifications and scheduled/retention-based backups.

## Quality and release

- [ ] Add migration, shift, return, backup, UI, and sanitizer tests; cheque, report, and audit service coverage is now present.
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
