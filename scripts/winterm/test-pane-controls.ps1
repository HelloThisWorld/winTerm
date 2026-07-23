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
    [switch]$RequireCompiled,

    [Parameter()]
    [switch]$SourceOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-Contains
{
    param(
        [Parameter(Mandatory)]
        [string]$Content,

        [Parameter(Mandatory)]
        [string]$Value,

        [Parameter(Mandatory)]
        [string]$Description
    )

    if (-not $Content.Contains($Value))
    {
        throw "$Description is missing '$Value'."
    }
}

try
{
    $root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $requiredFiles = @(
        'src\winterm\Actions\DirectedSplitAction.cpp',
        'src\winterm\PaneControls\PaneHeader.cpp',
        'src\winterm\PaneControls\PaneIcon.cpp',
        'src\winterm\PaneControls\PaneCommandModel.cpp',
        'src\winterm\PaneControls\PaneHeaderViewModel.cpp',
        'src\winterm\PaneControls\PaneHeaderSettings.cpp',
        'src\cascadia\UnitTests_SettingsModel\WinTermPaneControlsTests.cpp',
        'src\cascadia\TerminalApp\Pane.cpp',
        'src\cascadia\TerminalApp\Tab.cpp',
        'src\cascadia\TerminalApp\TerminalPage.cpp',
        'src\cascadia\TerminalApp\AppActionHandlers.cpp',
        'src\cascadia\TerminalSettingsModel\defaults.json'
    )
    foreach ($relativePath in $requiredFiles)
    {
        if (-not (Test-Path -LiteralPath (Join-Path $root $relativePath) -PathType Leaf))
        {
            throw "Pane control boundary '$relativePath' is missing."
        }
    }

    $directedSplit = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[0])
    foreach ($required in @(
        'DockZone::Top',
        'DockZone::Bottom',
        'DockZone::Left',
        'DockZone::Right',
        'focusedPaneId',
        'minimumPaneWidth',
        'minimumPaneHeight',
        '"splitPaneTop"',
        '"splitPaneBottom"',
        '"splitPaneLeft"',
        '"splitPaneRight"'
    ))
    {
        Assert-Contains $directedSplit $required 'Directed split boundary'
    }

    $paneIcon = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[2])
    foreach ($required in @(
        'PanePointerRegion::PaneIcon',
        'PanePointerRegion::HeaderBody',
        'PanePointerRegion::OverflowButton',
        'PaneMenuInvocation::IconRightClick'
    ))
    {
        Assert-Contains $paneIcon $required 'Pane icon behavior'
    }
    if ($paneIcon -match '(?i)drag|move pane')
    {
        throw 'The pane icon still exposes pane movement semantics.'
    }

    $menu = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[3])
    foreach ($required in @(
        'balancePanes',
        'closeFocusedPane',
        'DirectedSplitAction::CommandId'
    ))
    {
        Assert-Contains $menu $required 'Pane command model'
    }
    if ($menu -match '(?i)movePane|startPaneMove|dockPane|dragPane')
    {
        throw 'The pane command model still contains a pane movement command.'
    }

    $runtimePane = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[7])
    foreach ($required in @(
        '_AttachLeafVisual',
        'SetPaneHeadersVisible',
        '_paneIcon',
        'Open pane menu',
        'Focus this pane'
    ))
    {
        Assert-Contains $runtimePane $required 'Runtime pane header'
    }
    if ($runtimePane -match '(?i)PaneDrag|paneGrip|SizeAll|Start Pane Move|Drag to move')
    {
        throw 'The runtime pane header still contains drag-reposition behavior.'
    }

    $runtimeTab = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[8])
    if ($runtimeTab -match '(?i)_PaneDrag|PaneHandleDrag|PaneDockingOverlay|_DockPane')
    {
        throw 'The runtime tab still contains pane drag docking behavior.'
    }

    $terminalPage = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[9])
    foreach ($required in @(
        'SplitDirection::Up',
        'SplitDirection::Down',
        'SplitDirection::Left',
        'SplitDirection::Right',
        'BalancePanesText',
        'ShortcutAction::BalancePanes'
    ))
    {
        Assert-Contains $terminalPage $required 'Runtime pane menu'
    }
    if ($terminalPage -match 'MovePaneToNew(Tab|Window)Text')
    {
        throw 'The runtime pane menu still advertises pane movement.'
    }

    $actionHandlers = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[10])
    foreach ($required in @(
        '_HandleBalancePanes',
        '_HandleUndoPaneResize',
        '_HandleRedoPaneResize',
        '#if defined(WT_BRANDING_WINTERM)'
    ))
    {
        Assert-Contains $actionHandlers $required 'Pane action handler'
    }

    $defaultCommands = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[11])
    foreach ($required in @(
        '"id": "splitPaneTop"',
        '"id": "splitPaneBottom"',
        '"id": "splitPaneLeft"',
        '"id": "splitPaneRight"',
        '"id": "Terminal.BalancePanes"',
        '"id": "Terminal.UndoPaneResize"',
        '"id": "Terminal.RedoPaneResize"'
    ))
    {
        Assert-Contains $defaultCommands $required 'Command Palette command'
    }
    if ($defaultCommands -match '"action"\s*:\s*"movePane"')
    {
        throw 'Default commands still expose pane movement.'
    }

    $removedFiles = @(
        'src\winterm\Actions\MovePaneAction.cpp',
        'src\winterm\Docking\Drag\PaneHandleDragSource.cpp',
        'src\winterm\Docking\Overlay\PaneSnapLayoutOverlay.cpp',
        'src\winterm\Accessibility\KeyboardDockingController.cpp'
    )
    foreach ($relativePath in $removedFiles)
    {
        if (Test-Path -LiteralPath (Join-Path $root $relativePath))
        {
            throw "Removed pane movement component '$relativePath' still exists."
        }
    }

    $testBinary = Join-Path $root "bin\$Platform\$Configuration\UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll"
    if ($SourceOnly)
    {
        Write-Host 'SKIP: compiled pane control tests are handled by the parent suite.' -ForegroundColor Yellow
    }
    elseif (Test-Path -LiteralPath $testBinary -PathType Leaf)
    {
        & (Join-Path $PSScriptRoot 'test.ps1') -Suite Relevant -Configuration $Configuration -Platform $Platform
        if (-not $?)
        {
            throw 'Compiled pane control tests failed.'
        }
    }
    elseif ($RequireCompiled)
    {
        throw "Compiled Settings Model tests were not found at '$testBinary'."
    }
    else
    {
        Write-Host 'SKIP: compiled pane control tests are unavailable.' -ForegroundColor Yellow
    }

    Write-Host 'PASS: directed split remains available and pane movement UI is absent.' -ForegroundColor Green
}
catch
{
    Write-Error "Pane control tests failed: $($_.Exception.Message)"
    exit 1
}
