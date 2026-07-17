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
    $history = Get-Content -Raw -LiteralPath (Join-Path $root 'src\winterm\Docking\History\LayoutHistory.cpp')
    $tests = Get-Content -Raw -LiteralPath (Join-Path $root 'src\cascadia\UnitTests_SettingsModel\WinTermDockingSafetyTests.cpp')
    foreach ($required in @('_redo.clear()', 'SessionUnavailable', 'apply(candidate, true)', '_limit', '_trim'))
    {
        if (-not $history.Contains($required))
        {
            throw "Layout history boundary '$required' is missing."
        }
    }
    if (-not $tests.Contains('LayoutHistoryUndoRedoHonorsSessionLifetime'))
    {
        throw 'Compiled layout history coverage is missing.'
    }

    $testBinary = Join-Path $root "bin\$Platform\$Configuration\UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll"
    if (Test-Path -LiteralPath $testBinary -PathType Leaf)
    {
        & (Join-Path $PSScriptRoot 'test.ps1') -Suite Relevant -Configuration $Configuration -Platform $Platform
        if (-not $?)
        {
            throw 'Compiled layout history tests failed.'
        }
    }
    elseif ($RequireCompiled)
    {
        throw "Compiled Settings Model tests were not found at '$testBinary'."
    }
    else
    {
        Write-Host 'SKIP: compiled layout history tests are unavailable.' -ForegroundColor Yellow
    }
    Write-Host 'PASS: layout history source and test boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Layout history tests failed: $($_.Exception.Message)"
    exit 1
}
