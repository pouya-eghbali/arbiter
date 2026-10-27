# Upload from Windows to a draft only. Example: -Tag v0.1.0 -Platform windows
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Tag,
    [ValidateSet('macos', 'windows', 'linux')][string[]]$Platform,
    [string]$Assets = 'build-release-artifacts',
    [switch]$Unsigned
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Push-Location $root
try {
    $python = Join-Path $root 'build-release-python/Scripts/python.exe'
    if ($env:ARBITER_PYTHON) { $python = $env:ARBITER_PYTHON }
    elseif (!(Test-Path $python)) { $python = 'python' }
    $arguments = @('tools/release/publish.py', '--tag', $Tag, '--assets', $Assets)
    foreach ($item in $Platform) { $arguments += @('--platform', $item) }
    if ($Unsigned) { $arguments += '--unsigned' }
    & $python @arguments
    if ($LASTEXITCODE -ne 0) { throw "Release upload failed (exit $LASTEXITCODE)." }
} finally { Pop-Location }
