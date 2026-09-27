<#
Builds a clean, self-contained Windows installer for Invento.

Steps: Release build -> assemble dist/ -> windeployqt -> data-leak check ->
self-contained launch check -> Inno Setup compile -> Invento_Setup_<version>.exe

The installer contains PROGRAM FILES ONLY. The script aborts if any database,
backup, receipt, log or settings file is found in dist/.

Usage:  powershell -ExecutionPolicy Bypass -File tools\build-installer.ps1
Optional: -Version 1.2.3  -QtDir "C:\Qt\6.11.1\mingw_64"
#>
param(
    [string]$Version = "",
    [string]$QtDir = "",
    [switch]$SkipSelfTest
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Write-Host "Repo root: $root"

function Fail($msg) { Write-Host "`nBUILD FAILED: $msg" -ForegroundColor Red; exit 1 }

# --- Resolve toolchain -------------------------------------------------------
if (-not $QtDir) {
    $QtDir = Get-ChildItem "C:\Qt\6.*\mingw_64" -Directory -ErrorAction SilentlyContinue |
             Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $QtDir -or -not (Test-Path $QtDir)) { Fail "Qt mingw_64 not found. Pass -QtDir." }
$mingw = Get-ChildItem "C:\Qt\Tools\mingw*_64\bin" -Directory -ErrorAction SilentlyContinue |
         Sort-Object Name -Descending | Select-Object -First 1 -ExpandProperty FullName
$cmake = (Get-ChildItem "C:\Qt\Tools\CMake_64\bin\cmake.exe" -ErrorAction SilentlyContinue).FullName
$ninja = (Get-ChildItem "C:\Qt\Tools\Ninja\ninja.exe" -ErrorAction SilentlyContinue).FullName
if (-not $mingw) { Fail "MinGW toolchain not found under C:\Qt\Tools." }
if (-not $cmake) { $cmake = "cmake" }
if (-not $ninja) { $ninja = "ninja" }
$windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) { Fail "windeployqt not found in $QtDir\bin." }
$env:PATH = "$QtDir\bin;$mingw;" + (Split-Path $ninja -Parent) + ";" + $env:PATH
Write-Host "Qt:    $QtDir"
Write-Host "MinGW: $mingw"

# --- Version -----------------------------------------------------------------
if (-not $Version) {
    $cml = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
    if ($cml -match 'project\([^)]*VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)') { $Version = $Matches[1] }
    else { Fail "Could not read version from CMakeLists.txt; pass -Version." }
}
Write-Host "Version: $Version"

# --- Release build -----------------------------------------------------------
$build = Join-Path $root "build-release"
Write-Host "`n[1/6] Configuring Release build..."
& $cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPOS_BUILD_TESTS=OFF `
    "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_C_COMPILER=$mingw\gcc.exe" "-DCMAKE_CXX_COMPILER=$mingw\g++.exe" `
    "-DCMAKE_PREFIX_PATH=$QtDir"
if ($LASTEXITCODE -ne 0) { Fail "CMake configure failed." }
Write-Host "[2/6] Building invento.exe (Release)..."
& $cmake --build $build --target invento
if ($LASTEXITCODE -ne 0) { Fail "Release build failed." }

$exe = Join-Path $build "invento.exe"
if (-not (Test-Path $exe)) { Fail "Built exe not found at $exe." }

# --- Assemble dist + windeployqt --------------------------------------------
$dist = Join-Path $root "dist"
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Path $dist | Out-Null
Copy-Item $exe (Join-Path $dist "invento.exe")
Write-Host "[3/6] Running windeployqt..."
& $windeployqt --release --compiler-runtime --no-translations (Join-Path $dist "invento.exe")
if ($LASTEXITCODE -ne 0) { Fail "windeployqt failed." }

# --- Data-leak check (fail if the dist carries any data) ---------------------
Write-Host "[4/6] Checking dist for stray data..."
$bad = Get-ChildItem $dist -Recurse -Force | Where-Object {
    $_.Name -match '\.(db|db-wal|db-shm|sha256)$' -or $_.Name -in @('backups','receipts','logs','business.db')
}
if ($bad) {
    $bad | ForEach-Object { Write-Host "  LEAK: $($_.FullName)" -ForegroundColor Red }
    Fail "dist contains data files/folders. The installer must ship program files only."
}
Write-Host "  OK - no database, backups, receipts or logs in dist."

# --- Self-contained launch check (Qt NOT in PATH) ----------------------------
if (-not $SkipSelfTest) {
    Write-Host "[5/6] Verifying dist runs standalone (Qt scrubbed from PATH)..."
    $selfDir  = Join-Path $env:TEMP ("invento-selftest-" + [guid]::NewGuid().ToString("N"))
    $selfData = "$selfDir-data"
    Copy-Item $dist $selfDir -Recurse
    # Scrub Qt from PATH for the child (it inherits our env). PS 5.1 has no
    # Start-Process -Environment, so set-and-restore the current process PATH.
    $savedPath = $env:PATH
    $env:PATH = ($env:PATH -split ';' | Where-Object { $_ -and ($_ -notlike "*\Qt\*") }) -join ';'
    try {
        $p = Start-Process (Join-Path $selfDir "invento.exe") -ArgumentList "--data-dir=$selfData" -PassThru
        Start-Sleep -Seconds 5
        if ($p.HasExited) {
            Fail "dist exe exited immediately (exit $($p.ExitCode)) with Qt not in PATH -> a DLL/plugin is missing."
        }
        $p | Stop-Process -Force
        Write-Host "  OK - launched standalone."
    } finally {
        $env:PATH = $savedPath
        Remove-Item $selfDir,$selfData -Recurse -Force -ErrorAction SilentlyContinue
    }
} else { Write-Host "[5/6] Self-test skipped." }

# --- Inno Setup compile ------------------------------------------------------
Write-Host "[6/6] Compiling installer with Inno Setup..."
$iscc = @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { $iscc = (Get-Command iscc -ErrorAction SilentlyContinue).Source }
if (-not $iscc) {
    Write-Host "`ndist\ is ready and verified, but Inno Setup 6 is not installed." -ForegroundColor Yellow
    Write-Host "Install it from https://jrsoftware.org/isdl.php then re-run this script."
    exit 2
}
$iss = Join-Path $root "installer\invento.iss"
& $iscc "/DAppVersion=$Version" "/DDistDir=$dist" $iss
if ($LASTEXITCODE -ne 0) { Fail "Inno Setup compile failed." }

$setup = Join-Path $root "installer\dist-installer\Invento_Setup_$Version.exe"
if (-not (Test-Path $setup)) { Fail "Installer not produced." }
# Light check that no obvious db filename is embedded in the compiled installer.
$bytes = [System.IO.File]::ReadAllBytes($setup)
$ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
if ($ascii -match "business\.db") { Fail "Setup.exe appears to reference business.db - aborting." }
Write-Host "`nDONE. Installer: $setup" -ForegroundColor Green
