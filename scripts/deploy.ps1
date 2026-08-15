[CmdletBinding()]
param(
    [string]$BuildDir = "build-ui",
    [string]$OutputDir = "deploy",
    [string]$Executable = "invento.exe",
    [string]$WindeployQt = ""
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildPath = Join-Path $repo $BuildDir
$outputPath = Join-Path $repo $OutputDir
$exe = Join-Path $buildPath $Executable

if (-not (Test-Path $exe)) {
    cmake --build $buildPath --config Release
}
if (-not (Test-Path $exe)) { throw "Release executable was not produced: $exe" }

if ([string]::IsNullOrWhiteSpace($WindeployQt)) {
    $command = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if ($null -eq $command) { throw "windeployqt.exe was not found. Pass -WindeployQt with the Qt tool path." }
    $WindeployQt = $command.Source
}

if (Test-Path $outputPath) { Remove-Item -LiteralPath $outputPath -Recurse -Force }
New-Item -ItemType Directory -Path $outputPath | Out-Null
Copy-Item -LiteralPath $exe -Destination $outputPath
& $WindeployQt --release --no-translations --no-system-d3d-compiler (Join-Path $outputPath $Executable)
Copy-Item -LiteralPath (Join-Path $repo "installer/NexoraPOS.iss") -Destination $outputPath
Copy-Item -LiteralPath (Join-Path $repo "docs/SUPPORT.md") -Destination $outputPath
Write-Host "Deployment staging complete: $outputPath"
