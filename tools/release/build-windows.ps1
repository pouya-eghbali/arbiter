# Run in an x64 MSVC Developer PowerShell. Signing is on unless -Unsigned is used.
[CmdletBinding()]
param(
    [int]$Jobs = 24,
    [switch]$Unsigned,
    [string]$SdkPrefix
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $root
try {
    $python = Join-Path $root 'build-release-python/Scripts/python.exe'
    if ($env:ARBITER_PYTHON) { $python = $env:ARBITER_PYTHON }
    elseif (!(Test-Path $python)) { $python = 'python' }
    $arguments = @('tools/release/build.py', '--package', '--jobs', "$Jobs")
    if ($Unsigned) { $arguments += '--unsigned' }
    if ($SdkPrefix) { $arguments += @('--sdk-prefix', $SdkPrefix) }
    & $python @arguments
    if ($LASTEXITCODE -ne 0) { throw "Release build failed (exit $LASTEXITCODE)." }
} finally { Pop-Location }
