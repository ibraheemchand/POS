# Engineering Memory

- The database layer uses one SQLite connection in the current single-threaded UI. Background workers must use separate connections before being introduced.
- Migrations are embedded ordered SQL and must remain backward-compatible. Legacy column repair is performed by schema inspection because SQLite does not support portable `ADD COLUMN IF NOT EXISTS`.
- `Money` is integer paisa; UI formatting must avoid floating point.
- FBR is deliberately out of scope.
- Existing real UI pages are Inventory, Sales POS, Purchases, Customers, Suppliers, Cash Management, Cheques, Reports, Analytics, Audit Log, Settings, and Backup/Restore. Settings values are persisted in the local SQLite `settings` table through `SettingsService`; Analytics uses ReportService plus a parameterized seven-day sales query.
- Legacy schema repair is performed after ordered migrations by inspecting `PRAGMA table_info`; the startup gate uses `Database::isSchemaCompatible` and blocks an incomplete database.
- Cash-bearing sales, purchases, and customer payments now attach to an active shift; if none exists, the service creates a zero-opening active shift so cash is never unassigned. Cash Management exposes explicit open/close and difference reporting.
- `ReturnService` validates against previously returned quantities and reverses stock/batches atomically; sales returns reduce customer credit first and optionally refund remaining cash, while purchase returns reduce supplier payable.
- Customers now expose a service-backed payment allocation action; it selects an outstanding invoice and records the payment through `PaymentService` without direct business writes from the UI.
- `SupplierService::ledger()` returns ordered supplier ledger entries; the Suppliers page renders the selected supplier's debit, credit, reference, and balance history without writing directly from the UI.
- Reports can export the selected date range as local CSV or PDF using ReportService and Qt PrintSupport; CSV import and Excel output remain separate backlog items.
