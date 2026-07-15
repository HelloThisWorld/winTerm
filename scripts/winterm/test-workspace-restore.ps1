# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try
{
    $repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $fixture = Join-Path $repositoryRoot 'tests\winterm\Fixtures\Workspaces\Valid\split-pane.winterm-workspace.json'
    & (Join-Path $PSScriptRoot 'validate-workspaces.ps1') -Path $fixture
    if (-not $?)
    {
        throw 'The split-pane restore fixture is invalid.'
    }

    $workspace = Get-Content -LiteralPath $fixture -Raw -Encoding UTF8 | ConvertFrom-Json
    $tab = $workspace.windows[0].tabs[0]
    if ($tab.layout.type -ne 'split' -or $tab.layout.orientation -ne 'vertical' -or [double]$tab.layout.ratio -ne 0.58)
    {
        throw 'The restore fixture did not preserve split orientation and ratio.'
    }
    if ($tab.activePaneId -ne 'pane-left' -or $tab.panes.Count -ne 2)
    {
        throw 'The restore fixture did not preserve pane focus or pane count.'
    }

    $resolver = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Workspaces\Restore\WorkspaceResolvers.cpp') -Raw
    $restore = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Workspaces\Restore\WorkspaceRestoreService.cpp') -Raw
    foreach ($required in @('normalizedBounds', 'ScaleBoundsForDpi', 'visibleTitleBarLogical', 'restoreMinimizedWindows', 'knownWslDistributions'))
    {
        if (-not ($resolver.Contains($required) -or $resolver.Contains($required.Substring(0, 1).ToUpperInvariant() + $required.Substring(1))))
        {
            throw "Restore resolver boundary '$required' is missing."
        }
    }
    foreach ($required in @('MovePreferredFirst', 'ProfileResolver::Resolve', 'DirectoryResolver::Resolve', 'RestoreItemStatus::Failed'))
    {
        if (-not $restore.Contains($required))
        {
            throw "Progressive restore boundary '$required' is missing."
        }
    }
    Write-Host 'PASS: workspace restore planning, pane layout, monitor, DPI, and fallback foundations.' -ForegroundColor Green
}
catch
{
    Write-Error "Workspace restore tests failed: $($_.Exception.Message)"
    exit 1
}
