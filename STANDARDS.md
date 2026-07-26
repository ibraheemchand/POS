# Engineering Standards

- C++20, RAII, smart pointers, and no owning raw pointers.
- Store all money as `qint64` paisa. Do not use floating point for money calculations or formatting.
- Use parameterized SQLite statements only.
- Every operation touching stock, ledgers, cash, or invoice state uses one explicit RAII transaction.
- Preserve UUID primary keys, foreign keys, WAL mode, busy timeout, and append-only audit history.
- Validate external/UI input before service/database calls.
- Keep business logic free of Qt widget dependencies where practical.
- Add a focused QtTest regression for every financial workflow and rollback path.
- Run CMake build and CTest before declaring work complete; do not ignore warnings.
- Never add FBR code or a network dependency; this product is offline-only.
