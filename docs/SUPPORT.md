# Support and recovery runbook

## A normal backup

Use **Backup now** from the dashboard before any major change, and copy the resulting `.db` plus its `.sha256` file to a USB drive or a folder already synchronized by the customer's cloud desktop client. The application itself does not upload business data.

The Backup & Restore screen supports local or USB destinations, external `.db` restore selection, integrity verification, PIN authorization, safety backups, and retention pruning. Automatic backup intervals are configured in Settings and apply after restarting the application.

## Recovery after a database warning

1. Stop taking sales on the affected PC.
2. Keep the current `business.db`; do not overwrite or delete it.
3. Compare the backup's `.sha256` sidecar with the SHA-256 of its `.db` file.
4. Open a copy of the backup with a SQLite tool and run `PRAGMA integrity_check;`. It must return `ok`.
5. In Backup & Restore, choose **Restore from drive/file**, select the verified `.db`, complete the PIN prompt, and confirm the safety-backup warning. The application verifies the snapshot and creates a safety backup before replacing live data.
6. Restart the application and confirm the latest invoice, product count, and opening cash before resuming trade.

## Migration failure

Migrations are transactional. If startup fails during a migration, retain the database and application log, install the previous known-good program build, and restore the most recent verified backup. Do not attempt manual edits to `schema_version`.

## Demo data and deployment

Use `wholesale_pos.exe --seed-demo` only in a QA/support data directory. It is idempotent and never runs during normal startup. For a clean Windows package, run `scripts/deploy.ps1` and then build `installer/NexoraPOS.iss` with Inno Setup.

## FBR scope

FBR integration is intentionally excluded from this product. Core sales remain fully offline and no FBR credentials or network synchronization are required.
