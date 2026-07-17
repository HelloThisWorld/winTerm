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
    $transaction = Get-Content -Raw -LiteralPath (Join-Path $root 'src\winterm\Docking\Transactions\LayoutTransaction.cpp')
    $tests = Get-Content -Raw -LiteralPath (Join-Path $root 'src\cascadia\UnitTests_SettingsModel\WinTermDockingSafetyTests.cpp')
    foreach ($required in @(
        'SessionsLeased',
        'TargetPrepared',
        'OwnershipReserved',
        'ModelCommitted',
        'VisualCommitted',
        'RollingBack',
        'recoverSessions',
        'invalidateDragToken'
    ))
    {
        if (-not $transaction.Contains($required))
        {
            throw "Docking transaction boundary '$required' is missing."
        }
    }
    foreach ($required in @('TransactionCommitsTargetBeforeSourceRelease', 'TransactionFailureRestoresOwnership'))
    {
        if (-not $tests.Contains($required))
        {
            throw "Compiled transaction test '$required' is missing."
        }
    }

    $testBinary = Join-Path $root "bin\$Platform\$Configuration\UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll"
    if (Test-Path -LiteralPath $testBinary -PathType Leaf)
    {
        & (Join-Path $PSScriptRoot 'test.ps1') -Suite Relevant -Configuration $Configuration -Platform $Platform
        if (-not $?)
        {
            throw 'Compiled docking transaction tests failed.'
        }
    }
    elseif ($RequireCompiled)
    {
        throw "Compiled Settings Model tests were not found at '$testBinary'."
    }
    else
    {
        Write-Host 'SKIP: compiled docking transaction tests are unavailable.' -ForegroundColor Yellow
    }
    Write-Host 'PASS: docking transaction source and test boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Docking transaction tests failed: $($_.Exception.Message)"
    exit 1
}
