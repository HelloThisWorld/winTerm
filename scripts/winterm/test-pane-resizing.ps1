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
    $modelPath = Join-Path $root 'src\winterm\PaneResize\PaneResizeModel.cpp'
    $tokensPath = Join-Path $root 'src\winterm\Design\InteractionTokens.h'
    $runtimePath = Join-Path $root 'src\cascadia\TerminalApp\Pane.cpp'
    $testsPath = Join-Path $root 'src\cascadia\UnitTests_SettingsModel\WinTermPaneResizeTests.cpp'
    foreach ($path in @($modelPath, $tokensPath, $runtimePath, $testsPath))
    {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf))
        {
            throw "Pane resize boundary '$path' is missing."
        }
    }

    $tokens = Get-Content -Raw -LiteralPath $tokensPath
    foreach ($required in @(
        'DividerVisibleThickness',
        'DividerPointerHitThickness',
        'SnapThreshold',
        'SnapReleaseThreshold',
        'SnapIndicatorDuration',
        '0.25',
        '1.0 / 3.0',
        '0.5',
        '2.0 / 3.0',
        '0.75'
    ))
    {
        if (-not $tokens.Contains($required))
        {
            throw "Interaction token '$required' is missing."
        }
    }

    $model = Get-Content -Raw -LiteralPath $modelPath
    foreach ($required in @(
        'altDisablesSnapping',
        'snapReleaseThreshold',
        '_activeSnapRatio',
        'minimumFirst',
        'minimumSecond',
        'ResizeTransactionState::Cancelled',
        'PaneResizeHistory::Record',
        '_redo.clear()'
    ))
    {
        if (-not $model.Contains($required))
        {
            throw "Pane resize model boundary '$required' is missing."
        }
    }

    $runtime = Get-Content -Raw -LiteralPath $runtimePath
    foreach ($required in @(
        '_CreateDividerVisual',
        '_DividerPointerPressed',
        '_DividerPointerMoved',
        '_DividerPointerReleased',
        '_CancelDividerResize',
        'CoreCursorType::SizeWestEast',
        'CoreCursorType::SizeNorthSouth',
        'VirtualKey::Menu',
        'PaneResizeCommitted.raise'
    ))
    {
        if (-not $runtime.Contains($required))
        {
            throw "Runtime divider boundary '$required' is missing."
        }
    }
    if ([regex]::Matches($runtime, '_CreateDividerVisual\(\);').Count -lt 5)
    {
        throw 'Divider visuals are not reattached across all branch rebuild paths.'
    }

    $moveStart = $runtime.IndexOf('void Pane::_DividerPointerMoved')
    $moveEnd = $runtime.IndexOf('void Pane::_DividerPointerReleased', $moveStart)
    if ($moveStart -lt 0 -or $moveEnd -le $moveStart)
    {
        throw 'Runtime divider move handler could not be isolated.'
    }
    $moveHandler = $runtime.Substring($moveStart, $moveEnd - $moveStart)
    if ($moveHandler -match '(?i)serialize|workspace|writeall|file|autosave')
    {
        throw 'Pointer movement performs persistence work.'
    }

    $tests = Get-Content -Raw -LiteralPath $testsPath
    foreach ($required in @(
        'CommonRatiosSnapAndReportExpectedLabels',
        'InvalidSnapRatiosAreFilteredByMinimumSizes',
        'AltBypassesSnapping',
        'HysteresisUsesLargerReleaseThreshold',
        'CancelRestoresOriginalRatio',
        'NestedResizeUsesOwningSplitGeometry',
        'LogicalThresholdIsStableAcrossDpiScales',
        'OneCommittedDragCreatesOneUndoEntry',
        'NewResizeClearsRedo'
    ))
    {
        if (-not $tests.Contains($required))
        {
            throw "Pane resize regression '$required' is missing."
        }
    }

    $testBinary = Join-Path $root "bin\$Platform\$Configuration\UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll"
    if ($SourceOnly)
    {
        Write-Host 'SKIP: compiled pane resize tests are handled by the parent suite.' -ForegroundColor Yellow
    }
    elseif (Test-Path -LiteralPath $testBinary -PathType Leaf)
    {
        & (Join-Path $PSScriptRoot 'test.ps1') -Suite Relevant -Configuration $Configuration -Platform $Platform
        if (-not $?)
        {
            throw 'Compiled pane resize tests failed.'
        }
    }
    elseif ($RequireCompiled)
    {
        throw "Compiled Settings Model tests were not found at '$testBinary'."
    }
    else
    {
        Write-Host 'SKIP: compiled pane resize tests are unavailable.' -ForegroundColor Yellow
    }

    Write-Host 'PASS: pane resize snapping, cancellation, DPI, and history source boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Pane resize tests failed: $($_.Exception.Message)"
    exit 1
}
