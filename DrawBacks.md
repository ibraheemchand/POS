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
