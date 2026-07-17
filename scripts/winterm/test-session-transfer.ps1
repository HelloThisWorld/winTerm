# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [Parameter()]
    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [switch]$RequireCompiled
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try
{
    $root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $ownership = Get-Content -Raw -LiteralPath (Join-Path $root 'src\winterm\Docking\Sessions\SessionOwnership.cpp')
    $spike = Get-Content -Raw -LiteralPath (Join-Path $root 'docs\visual-docking-spike.md')
    $tests = Get-Content -Raw -LiteralPath (Join-Path $root 'src\cascadia\UnitTests_SettingsModel\WinTermDockingSafetyTests.cpp')
    foreach ($required in @('AcquireLease', 'leaseCount', 'transferableWithinProcess', 'ValidateExactlyOneOwner', 'sourceInstanceId'))
    {
        if (-not $ownership.Contains($required))
        {
            throw "Session transfer boundary '$required' is missing."
        }
    }
    if (-not $spike.Contains('no-go for arbitrary cross-process pane transfer'))
    {
        throw 'The cross-process live-transfer limitation is not documented.'
    }
    foreach ($required in @('SessionLeasePreventsPrematureRemoval', 'SessionTransferRejectsAnotherProcess'))
    {
        if (-not $tests.Contains($required))
        {
            throw "Compiled session transfer test '$required' is missing."
        }
    }

    $testBinary = Join-Path $root "bin\$Platform\$Configuration\UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll"
    if (Test-Path -LiteralPath $testBinary -PathType Leaf)
    {
        & (Join-Path $PSScriptRoot 'test.ps1') -Suite Relevant -Configuration $Configuration -Platform $Platform
        if (-not $?)
        {
            throw 'Compiled session transfer tests failed.'
        }
    }
    elseif ($RequireCompiled)
    {
        throw "Compiled Settings Model tests were not found at '$testBinary'."
    }
    else
    {
        Write-Host 'SKIP: compiled session transfer tests are unavailable.' -ForegroundColor Yellow
    }
    Write-Host 'PASS: session transfer source, test, and capability boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Session transfer tests failed: $($_.Exception.Message)"
    exit 1
}
