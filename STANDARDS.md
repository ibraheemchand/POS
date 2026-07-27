# Engineering Standards

- C++20, RAII, smart pointers, and no owning raw pointers.
- Store all money as `qint64` paisa. Do not use floating point for money calculations or formatting.
- Use parameterized SQLite statements only.
- Every operation touching stock, ledgers, cash, or invoice state uses one explicit RAII transaction.
- Preserve UUID primary keys, foreign keys, WAL mode, busy timeout, and append-only audit history.
- Validate external/UI input before service/database calls.
- Keep business logic free of Qt widget dependencies where practical.
- Add a focused QtTest regression for every financial workflow and rollback path.
- CSV imports must parse quoted fields, validate all rows before writing, and commit the batch in one transaction.
- PINs must never be persisted in plaintext; use a per-record salt and a cryptographic digest through SecurityService.
- Operational notifications are emitted only after the business transaction commits and must be best-effort so notification storage cannot invalidate financial state.
- Raw ESC/POS output must reject control characters in user text and validate barcode length/content before writing to a configured device path.
- Run CMake build and CTest before declaring work complete; do not ignore warnings.
- Never add FBR code or a network dependency; this product is offline-only.
