[CmdletBinding()]
param([string]$BuildDir = "build-sanitize", [string]$CMake = "", [string]$Generator = "MinGW Makefiles", [string]$CCompiler = "", [string]$CxxCompiler = "", [string]$QtPrefix = "")

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($CMake)) {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($null -eq $command) { throw "cmake.exe was not found. Pass -CMake with the CMake executable path." }
    $CMake = $command.Source
}
$ctest = Join-Path (Split-Path -Parent $CMake) "ctest.exe"
$configureArgs = @('-S','.', '-B',$BuildDir, '-G',$Generator, '-DPOS_BUILD_TESTS=ON', '-DPOS_ENABLE_SANITIZERS=ON')
if ($CCompiler) { $configureArgs += "-DCMAKE_C_COMPILER=$CCompiler" }
if ($CxxCompiler) { $configureArgs += "-DCMAKE_CXX_COMPILER=$CxxCompiler" }
if ($QtPrefix) { $configureArgs += "-DCMAKE_PREFIX_PATH=$QtPrefix" }
& $CMake @configureArgs
if ($LASTEXITCODE -ne 0) { throw "Sanitizer CMake configuration failed ($LASTEXITCODE)." }
& $CMake --build $BuildDir --target pos_core_tests --parallel 4
if ($LASTEXITCODE -ne 0) { throw "Sanitizer build failed ($LASTEXITCODE)." }
& $ctest --test-dir $BuildDir -R '^pos_core_tests$' --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Sanitizer tests failed ($LASTEXITCODE)." }
