# Support and recovery runbook

## A normal backup

Use **Backup now** from the dashboard before any major change, and copy the resulting `.db` plus its `.sha256` file to a USB drive or a folder already synchronized by the customer's cloud desktop client. The application itself does not upload business data.

Automatic schedules and the full restore screen are the next UI slice. Until then, the included backup button creates an online SQLite snapshot that does not require closing the till.

## Recovery after a database warning

1. Stop taking sales on the affected PC.
2. Keep the current `business.db`; do not overwrite or delete it.
3. Compare the backup's `.sha256` sidecar with the SHA-256 of its `.db` file.
4. Open a copy of the backup with a SQLite tool and run `PRAGMA integrity_check;`. It must return `ok`.
5. With Nexora closed, make a dated copy of the current database, then replace `business.db` in the app-data folder with the verified backup.
6. Restart the application and confirm the latest invoice, product count, and opening cash before resuming trade.

## Migration failure

Migrations are transactional. If startup fails during a migration, retain the database and application log, install the previous known-good program build, and restore the automatic pre-migration backup once that scheduled feature is enabled. Do not attempt manual edits to `schema_version`.

## FBR scope

FBR reporting remains optional and must be validated by the customer’s accountant for their classification. Core sales must continue offline even when the optional reporting queue cannot synchronize.
