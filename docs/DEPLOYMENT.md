# Windows deployment

1. Build the Release executable with CMake.
2. Run `scripts/deploy.ps1 -WindeployQt <path-to-windeployqt.exe>` from the repository root. The script creates a clean `deploy/` directory, copies the executable, and runs `windeployqt` with translations and system D3D compiler files disabled.
3. Test `deploy/wholesale_pos.exe` on a clean Windows account. Confirm startup creates the per-user database, schema compatibility passes, and a backup/restore smoke check succeeds.
4. Open `deploy/NexoraPOS.iss` with Inno Setup to create the installer. The installer keeps business data in Qt's per-user app-data location, so upgrades do not overwrite the database.

The deployment script never copies a live database. Customer data must be backed up through the application and transferred separately using the verified `.db` and `.sha256` files.
