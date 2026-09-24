<#
  run-tests.ps1 — build and run the whole automated test suite.

  Runs:
    1. Business-logic tests (pos_core_tests)  — sales, returns, commissions,
       partners, backup/restore, reports, settings persistence, ...
    2. UI tests (pos_ui_smoke_tests)          — navigation, keyboard shortcuts,
       Quick Access, password gating, layout scan, screenshots.
    3. A DPI screenshot matrix at 100% / 125% / 150% for 1366x768 and 1920x1080.

  All artifacts land in .\test-output (screenshots per DPI, layout-report.txt).
  Never touches the real app database — every test uses a fresh temp DB.

  Usage:   powershell -ExecutionPolicy Bypass -File tools\run-tests.ps1
           powershell -ExecutionPolicy Bypass -File tools\run-tests.ps1 -SkipBuild
#>
param([switch]$SkipBuild)

$ErrorActionPreference = "Stop"
$root  = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build-fix"
$out   = Join-Path $root "test-output"

# Qt / MinGW toolchain on PATH (adjust here if Qt is installed elsewhere).
$qt = "C:\Qt\6.11.1\mingw_64"
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\mingw1310_64\bin;$qt\bin;" + $env:PATH

if (-not $SkipBuild) {
    Write-Host "== Building ==" -ForegroundColor Cyan
    cmake --build $build --target pos_core_tests pos_ui_smoke_tests
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
}

# Fresh artifacts folder each run.
if (Test-Path $out) { Remove-Item $out -Recurse -Force -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force -Path $out | Out-Null
$env:TEST_OUTPUT_DIR = $out

Write-Host "== Business-logic + UI tests (ctest) ==" -ForegroundColor Cyan
ctest --test-dir $build --output-on-failure
$ctestExit = $LASTEXITCODE

Write-Host "== Layout verification at 100/125/150% DPI (all four logical sizes each) ==" -ForegroundColor Cyan
$ui = Join-Path $build "pos_ui_smoke_tests.exe"
foreach ($dpi in @(@{f="1";l="100"}, @{f="1.25";l="125"}, @{f="1.5";l="150"})) {
    Write-Host ("  verifying at {0}%" -f $dpi.l)
    $env:QT_SCALE_FACTOR = $dpi.f
    $env:TEST_DPI_LABEL  = $dpi.l
    & $ui verifyLayoutAtRequiredSizes | Out-Null
}
Remove-Item Env:\QT_SCALE_FACTOR -ErrorAction SilentlyContinue
Remove-Item Env:\TEST_DPI_LABEL -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "Artifacts:" -ForegroundColor Green
Write-Host "  Screenshots : $out\screens\dpi{100,125,150}\"
$report = Join-Path $out "layout-report.txt"
if ((Test-Path $report) -and (Get-Item $report).Length -gt 0) {
    Write-Host "  Layout WARNINGS found (review):" -ForegroundColor Yellow
    Get-Content $report
} else {
    Write-Host "  Layout scan : clean (no horizontal overflow)" -ForegroundColor Green
}
if ($ctestExit -ne 0) { Write-Host "SOME TESTS FAILED" -ForegroundColor Red; exit 1 }
Write-Host "ALL TESTS PASSED" -ForegroundColor Green
