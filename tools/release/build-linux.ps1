# Docker Desktop must be running Linux containers on the Windows x64 machine.
[CmdletBinding()]
param([int]$Jobs = 24)
$ErrorActionPreference = 'Stop'
$python = if ($env:ARBITER_PYTHON) { $env:ARBITER_PYTHON } else { 'python' }
& $python (Join-Path $PSScriptRoot 'build-linux.py') --jobs $Jobs
if ($LASTEXITCODE -ne 0) { throw "Linux Docker build failed (exit $LASTEXITCODE)." }
