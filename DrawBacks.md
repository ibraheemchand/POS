# Project Limitations and Incomplete Tasks (Drawbacks)

During the codebase audit and review of the system backlog (`TASKS.md`), the following missing features, incomplete tasks, and blocked workflows have been identified:

## 1. Missing UI Workflows for Returns
* **Description**: While the core transactional logic and rollback safety for sales and purchase returns/refunds are fully implemented in `ReturnService`, the actual visual user interfaces are missing.
* **Impact**: Operators cannot initiate or view product return transactions directly from the application UI.
* **Ref**: `TASKS.md` Line 45.

## 2. Incomplete Cheque Reversal Accounting
* **Description**: There is currently no linkage between individual cheques and their originating sales, purchases, or customer payments. As a result, cheque status transitions (such as a cheque bouncing) do not automatically trigger atomic ledger or cash ledger reversals.
* **Impact**: Outstanding balances and ledger accounts might become desynchronized if a cheque fails to clear.
* **Blocker**: A formal business rule decision is pending regarding how bounced/cleared cheque settlements should be processed.
* **Ref**: `TASKS.md` Line 34.

## 3. Lack of Database Encryption & Offline Licensing
* **Description**: The database uses standard offline SQLite, but SQLCipher (for full database encryption at rest) and offline software licensing modules have not been integrated.
* **Impact**: The database is stored in plain text locally on the operator's machine.
* **Blocker**: Decisions regarding SQLCipher distribution channels and specific licensing policies are outstanding.
* **Ref**: `TASKS.md` Line 46.

## 4. Incomplete Test Coverage for Environment Restrictions
* **Description**:
  * **E2E UI Testing**: Desktop cursor/keyboard E2E coverage is missing due to the lack of a test runner that can safely acquire a stable window handle on Windows.
  * **Sanitizers**: ASan (AddressSanitizer) and UBSan (UndefinedBehaviorSanitizer) test checks are disabled because the standard MinGW toolchain provided in the environment does not bundle the necessary runtime libraries.
* **Ref**: `TASKS.md` Lines 40, 41.

## 5. Weak Input Validation on Customer/Supplier Creation Forms
* **Location**: `src/ui/pages/customers_page.cpp:57-74`, `src/ui/pages/suppliers_page.cpp` line 74
* **Description**: Customer and supplier CRUD uses sequential `QInputDialog::getText/getInt` prompts with only `isEmpty()` checks on names. No validation for:
  - Phone number format (accepts any string, no digit check)
  - Credit limit/payment terms bounds are soft (spinner allows 0-1000000000 paisa)
  - No duplicate name/phone detection before creation
* **Impact**: Users can create malformed customer records (e.g., "123" as phone, negative credit terms) that pollute the database and cause confusion in payment workflows.
* **Severity**: Medium — affects data quality and user experience, not data loss.
* **Recommendation**: Add field validators before creation; confirm data before saving; add duplicate checks.

## 6. Missing Invoice Selection UI for Customer Payments
* **Location**: `src/ui/pages/customers_page.cpp:99-104`
* **Description**: Payment allocation uses `QInputDialog::getItem` with text like `"INV-123 (due 5000 paisa)"`. Parsing amounts from display text is brittle and error-prone. No visual representation of invoice date, amount, or aging.
* **Impact**: Users must manually parse due amounts from text, risking payment allocation errors.
* **Severity**: Medium — affects payment accuracy and operator confidence.
* **Recommendation**: Replace with a dedicated payment dialog showing a table of unpaid invoices (date, amount, due, aging) with checkbox selection and visual total.

## 7. Unvalidated Product Image Paths
* **Location**: `src/ui/pages/inventory_page.cpp:324` (CSV import), inventory create/edit dialogs
* **Description**: Product image paths are stored as file strings (e.g., "C:/images/product.jpg") with no validation that the file exists, is readable, or is an actual image. Missing files will fail silently or show broken image placeholders.
* **Impact**: Products with invalid image paths will render as blank, confusing operators and potentially breaking product search/identify workflows.
* **Severity**: Low — UX issue, not functional.
* **Recommendation**: Validate image existence on create/update; support embedded image data or a centralized image directory; show a placeholder icon if missing.

## 8. Incomplete Purchase Order Workflow UI
* **Location**: `src/ui/pages/purchases_page.cpp` lines 40-100+
* **Description**: The purchases page provides supplier and product selection via combos, but the workflow to create and submit a purchase order is unclear. The page allows adding items but lacks a clear "New Purchase" button, cart, submission, or receiving flow UI.
* **Impact**: New operators may be unclear on how to initiate a purchase or receive stock.
* **Severity**: Medium — usability/training issue.
* **Recommendation**: Add explicit "New Purchase Order" → "Add items" → "Review & Submit" workflow with a visual cart/summary before commit.

## 9. Generic Error Handling with Silent Failures
* **Location**: `src/ui/pages/main_page.cpp:143-250` (all metric load blocks use `catch(...)` without logging)
* **Description**: Dashboard metrics silently swallow all exceptions and render fallback values (0 or "PKR 0.00"). If a report query fails, the operator sees "0" with no indication of the actual problem (e.g., corrupted table, permission error).
* **Impact**: Silent data display failures may go unnoticed; operators may make decisions based on incorrect "0" metrics.
* **Severity**: Medium-High — affects data visibility and decision-making.
* **Recommendation**: Log errors (even to a status bar); show a warning icon or "(error)" suffix if a metric fails to load; consider a "Refresh" button to retry.

## 10. Missing Cheque-to-Transaction Linkage
* **Location**: `src/ui/pages/customers_page.cpp:103` (cheque payment method), no linkage in cheque_service.cpp
* **Description**: Customers can record cheque payments, and the app has a cheque register, but a cheque payment is not linked back to the original sale or customer payment. If a cheque bounces or clears, there's no way to trace it back to reverse the sale or update the customer balance.
* **Impact**: Partial or full cheque reconciliation workflows are not possible; if a cheque bounces, manual journal entries or database corrections are required.
* **Severity**: High — blocks a critical cash management workflow.
* **Recommendation**: Add `payment_id` or `sale_id` foreign key to cheques table; update cheque reversal UI/logic to cascade reversals to sales and customer balances.

## 11. Missing Product Search Error Recovery
* **Location**: `src/ui/pages/sales_pos_page.cpp:87-90` (product search input)
* **Description**: Product search by name/SKU/barcode has no validation or feedback if the search query returns no results. Operator scans a barcode, nothing appears, and they're left uncertain if the product doesn't exist or the barcode is invalid.
* **Impact**: Slows checkout; operator confusion when products are not found (especially for new/imported products).
* **Severity**: Low-Medium — usability/training issue.
* **Recommendation**: Show "No products found" message if search is empty; add a "Create new product" quick link if operator scans an unknown barcode.

## 12. No Transaction Audit Trail for Sensitive Settings Changes
* **Location**: `src/ui/pages/settings_page.cpp:129-140` (settings save)
* **Description**: Business name, currency, printer path, and security PIN can be changed, but no audit log is created. Changes are not tracked for compliance/accountability.
* **Impact**: No ability to audit who changed printer paths or security settings; historical reconciliation is impossible.
* **Severity**: Medium — audit/compliance issue.
* **Recommendation**: Log all settings changes (including old/new values and timestamp) to the audit log when saved.
