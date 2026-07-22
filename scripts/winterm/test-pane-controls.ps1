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

try
{
    $root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $requiredFiles = @(
        'src\winterm\Actions\DirectedSplitAction.cpp',
        'src\winterm\Actions\MovePaneAction.cpp',
        'src\winterm\PaneControls\PaneHeader.cpp',
        'src\winterm\PaneControls\PaneHandle.cpp',
        'src\winterm\PaneControls\PaneCommandModel.cpp',
        'src\winterm\PaneControls\PaneHeaderViewModel.cpp',
        'src\winterm\PaneControls\PaneHeaderSettings.cpp',
        'src\winterm\Docking\Drag\PaneHandleDragSource.cpp',
        'src\winterm\Docking\Overlay\PaneSnapLayoutOverlay.cpp',
        'src\cascadia\UnitTests_SettingsModel\WinTermPaneControlsTests.cpp',
        'src\cascadia\TerminalApp\Pane.cpp',
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
        'LayoutTransformer::BuildProposedLayout',
        'focusedPaneId',
        'minimumPaneWidth',
        'minimumPaneHeight'
    ))
    {
        if (-not $directedSplit.Contains($required))
        {
            throw "Directed split boundary '$required' is missing."
        }
    }

    $handle = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[3])
    if (-not ($handle.Contains('PanePointerRegion::DragGrip') -and
        $handle.Contains('PanePointerRegion::OverflowButton')) -or
        $handle.Contains('TerminalContent, true'))
    {
        throw 'Pane Handle input isolation or overflow menu routing is incomplete.'
    }

    $menu = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[4])
    foreach ($required in @(
        'movePaneToNewTab',
        'movePaneToNewWindow',
        'closeFocusedPane',
        'DirectedSplitAction::CommandId',
        'disabledReason'
    ))
    {
        if (-not $menu.Contains($required))
        {
            throw "Pane menu command '$required' is missing."
        }
    }
    foreach ($required in @(
        '"splitPaneTop"',
        '"splitPaneBottom"',
        '"splitPaneLeft"',
        '"splitPaneRight"'
    ))
    {
        if (-not $directedSplit.Contains($required))
        {
            throw "Directed split command '$required' is missing."
        }
    }

    $drag = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[7])
    foreach ($required in @(
        'DragThreshold::Exceeded',
        'LayoutTransformer::BuildProposedLayout',
        'PaneSnapLayoutOverlay::Build',
        'DockPreview::Build',
        'DragCancellationReason'
    ))
    {
        if (-not $drag.Contains($required))
        {
            throw "Pane Handle drag boundary '$required' is missing."
        }
    }

    $runtimeHeader = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[10])
    foreach ($required in @(
        '_AttachLeafVisual',
        'PaneHeaderHeight',
        'SetPaneHeadersVisible',
        'ContextFlyout',
        'Move ',
        'Open pane menu',
        '_paneGrip.CapturePointer',
        'PaneDragPressed.raise',
        'PaneDragUpdated.raise',
        'PaneDragCompleted.raise',
        'PaneDragCancelled.raise'
    ))
    {
        if (-not $runtimeHeader.Contains($required))
        {
            throw "Runtime Pane Header boundary '$required' is missing."
        }
    }

    $terminalPage = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[11])
    foreach ($required in @(
        'SplitDirection::Up',
        'SplitDirection::Down',
        'SplitDirection::Left',
        'SplitDirection::Right',
        'MovePaneToNewTabText',
        'MovePaneToNewWindowDisabledText'
    ))
    {
        if (-not $terminalPage.Contains($required))
        {
            throw "Runtime directed split or Pane menu boundary '$required' is missing."
        }
    }

    $runtimeTab = Get-Content -Raw -LiteralPath (Join-Path $root 'src\cascadia\TerminalApp\Tab.cpp')
    foreach ($required in @(
        '_ResolvePaneDockTarget',
        '_ShowPaneDockingOverlay',
        '_DockPane',
        'PaneHandleDragSource',
        'DockTargetResolver',
        'LayoutTransactionCoordinator',
        'PaneSnapLayoutOverlay::Build',
        'PaneMoveToNewTabRequested.raise',
        'SplitDirection::Up',
        'SplitDirection::Down',
        'SplitDirection::Left',
        'SplitDirection::Right'
    ))
    {
        if (-not $runtimeTab.Contains($required))
        {
            throw "Runtime Pane Handle docking boundary '$required' is missing."
        }
    }

    $actionHandlers = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[12])
    $handlerStart = $actionHandlers.IndexOf('void TerminalPage::_HandleSplitPane')
    $handlerEnd = $actionHandlers.IndexOf('void TerminalPage::_HandleToggleSplitOrientation', $handlerStart)
    if ($handlerStart -lt 0 -or $handlerEnd -le $handlerStart)
    {
        throw 'The runtime Split Pane handler is missing.'
    }
    $splitHandler = $actionHandlers.Substring($handlerStart, $handlerEnd - $handlerStart)
    if ($splitHandler.IndexOf('PreCalculateCanSplit') -lt 0 -or
        $splitHandler.IndexOf('PreCalculateCanSplit') -gt $splitHandler.IndexOf('_MakePane(realArgs.ContentArgs()'))
    {
        throw 'Split capability validation must run before the new shell pane is created.'
    }

    $defaultCommands = Get-Content -Raw -LiteralPath (Join-Path $root $requiredFiles[13])
    foreach ($required in @(
        '"id": "splitPaneTop"',
        '"id": "splitPaneBottom"',
        '"id": "splitPaneLeft"',
        '"id": "splitPaneRight"',
        '"id": "movePaneToNewTab"',
        '"id": "movePaneToNewWindow"',
        '"id": "closeFocusedPane"',
        '"id": "openPaneMenu"'
    ))
    {
        if (-not $defaultCommands.Contains($required))
        {
            throw "Command Palette command $required is missing."
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
    Write-Host 'PASS: directed split and pane control source boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Pane control tests failed: $($_.Exception.Message)"
    exit 1
}
