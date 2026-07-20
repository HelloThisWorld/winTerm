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
        'split-pane-top',
        'split-pane-bottom',
        'split-pane-left',
        'split-pane-right',
        'split-focused-nested-pane',
        'split-with-selected-profile',
        'pane-header-automatic-visibility',
        'pane-header-overflow-menu',
        'pane-handle-context-menu',
        'pane-handle-drag-top',
        'pane-handle-drag-bottom',
        'pane-handle-drag-left',
        'pane-handle-drag-right',
        'pane-handle-drag-corner',
        'pane-handle-drag-tab-strip',
        'pane-handle-drag-new-window',
        'drag-pane-to-empty-slot',
        'terminal-content-drag-does-not-move-pane',
        'move-pane-to-new-tab',
        'move-pane-to-new-window',
        'close-pane-preserves-confirmation',
        'cancel-with-escape',
        'cancel-on-pointer-capture-loss',
        'keyboard-docking',
        'undo-redo',
        'workspace-restore',
        'mixed-dpi-pane-drag',
        'narrator-pane-controls',
        'high-contrast-pane-controls'
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

    throw 'The v0.7 runtime adapter has not passed the local UI automation gate yet.'
}
catch
{
    Write-Error "Docking UI validation failed: $($_.Exception.Message)"
    exit 1
}
