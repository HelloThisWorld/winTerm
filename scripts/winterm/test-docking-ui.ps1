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
    [ValidateSet('Validate', 'Run')]
    [string]$Mode = 'Validate',

    [Parameter()]
    [switch]$RequireRuntime
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try
{
    $root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $manifestPath = Join-Path $root 'tests\winterm\Docking\ui-automation.scenarios.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $expected = @(
        'drag-tab-within-window',
        'drag-tab-to-another-window',
        'drag-tab-to-edge',
        'drag-pane-to-empty-slot',
        'cancel-with-escape',
        'keyboard-docking',
        'undo-redo'
    )
    foreach ($scenario in $expected)
    {
        if (@($manifest.scenarios) -notcontains $scenario)
        {
            throw "UI automation scenario '$scenario' is missing."
        }
    }
    if ($manifest.execution -ne 'manual-local' -or
        $manifest.privacy.captureTerminalOutput -or
        $manifest.privacy.captureCommandText -or
        $manifest.privacy.captureClipboard -or
        $manifest.privacy.useRealUserWorkspace)
    {
        throw 'The local UI suite privacy or execution boundary is invalid.'
    }

    if ($Mode -eq 'Validate')
    {
        Write-Host 'PASS: local docking UI automation manifest and privacy boundaries.' -ForegroundColor Green
        Write-Host 'SKIP: multi-window UI automation was not executed.' -ForegroundColor Yellow
        return
    }

    $candidateExecutables = @(
        (Join-Path $root "bin\$Platform\$Configuration\CascadiaPackage\winTerm.exe"),
        (Join-Path $root "bin\$Platform\$Configuration\WindowsTerminal.exe")
    )
    $application = $candidateExecutables | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    $winAppDriver = Get-Command WinAppDriver.exe -ErrorAction SilentlyContinue
    if ($null -eq $application -or $null -eq $winAppDriver)
    {
        $message = 'The built winTerm application and WinAppDriver are required for the local UI suite.'
        if ($RequireRuntime)
        {
            throw $message
        }
        Write-Host "SKIP: $message" -ForegroundColor Yellow
        return
    }

    throw 'The v0.5 XAML docking adapter is feature-disabled; runtime UI automation cannot run yet.'
}
catch
{
    Write-Error "Docking UI validation failed: $($_.Exception.Message)"
    exit 1
}
