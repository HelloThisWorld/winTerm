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

    # Divider visuals are reattached through every branch rebuild path:
    # split creation/workspace restore (the split-parent constructor), swap,
    # close/collapse, and zoom restore. Attribute each call site to its
    # enclosing column-0 definition instead of relying on line numbers.
    $dividerRebuildOwners = [System.Collections.Generic.HashSet[string]]::new()
    $currentFunction = ''
    foreach ($line in ($runtime -split "`r?`n"))
    {
        if ($line -match '^(?=\S).*\bPane::([A-Za-z_~]+)\s*\(')
        {
            $currentFunction = $Matches[1]
        }
        if ($line -match '_CreateDividerVisual\(\);')
        {
            [void]$dividerRebuildOwners.Add($currentFunction)
        }
    }
    foreach ($required in @('Pane', 'SwapPanes', '_CloseChild', '_Split', 'Restore'))
    {
        if (-not $dividerRebuildOwners.Contains($required))
        {
            throw "Divider visuals are not reattached in the '$required' rebuild path."
        }
    }

    # v1.1.2 ghost-divider regression: the visible divider, the pointer/hit
    # targets, and the logical split position must share one leading-edge
    # coordinate system inside Pane::_UpdateDividerPlacement. A primary-axis
    # Center alignment re-centers the visual in the space left after its
    # margin and paints a ghost line away from the real split boundary.
    $placementMarker = 'void Pane::_UpdateDividerPlacement'
    $placementStart = $runtime.IndexOf($placementMarker)
    if ($placementStart -lt 0)
    {
        throw 'Divider placement routine could not be located.'
    }
    $nextDefinition = [regex]::Match(
        $runtime.Substring($placementStart + $placementMarker.Length),
        '(?m)^(?=\S).*\bPane::[A-Za-z_~]+\s*\(')
    $placement = if ($nextDefinition.Success)
    {
        $runtime.Substring($placementStart, $placementMarker.Length + $nextDefinition.Index)
    }
    else
    {
        $runtime.Substring($placementStart)
    }

    $verticalStart = $placement.IndexOf('SplitState::Vertical')
    if ($verticalStart -lt 0)
    {
        throw 'Divider placement vertical branch could not be located.'
    }
    $elseMatch = [regex]::Match($placement.Substring($verticalStart), '(?m)^\s*else\b')
    if (-not $elseMatch.Success)
    {
        throw 'Divider placement horizontal branch could not be located.'
    }
    $verticalBranch = $placement.Substring($verticalStart, $elseMatch.Index)
    $horizontalBranch = $placement.Substring($verticalStart + $elseMatch.Index)

    $alignmentOf = {
        param([string]$branch, [string]$element, [string]$axis)
        $match = [regex]::Match(
            $branch,
            [string]::Format('{0}\s*\.\s*{1}Alignment\s*\(\s*{1}Alignment::(\w+)', $element, $axis))
        if (-not $match.Success)
        {
            throw "Divider placement does not set the $axis alignment of $element."
        }
        $match.Groups[1].Value
    }

    # Vertical split: every divider element shares the leading-edge (Left)
    # primary-axis alignment model of the hit target.
    $verticalHit = & $alignmentOf $verticalBranch '_dividerHitTarget' 'Horizontal'
    $verticalPointer = & $alignmentOf $verticalBranch '_dividerPointerTarget' 'Horizontal'
    $verticalVisual = & $alignmentOf $verticalBranch '_dividerVisual' 'Horizontal'
    if ($verticalHit -ne 'Left')
    {
        throw "Vertical divider hit target must use leading-edge Left alignment, found '$verticalHit'."
    }
    if ($verticalPointer -ne $verticalHit -or $verticalVisual -ne $verticalHit)
    {
        throw "Vertical divider visuals must share the hit target's leading-edge alignment (hit '$verticalHit', pointer '$verticalPointer', visible '$verticalVisual')."
    }

    # Horizontal split: same invariant on the vertical (Top) primary axis.
    $horizontalHit = & $alignmentOf $horizontalBranch '_dividerHitTarget' 'Vertical'
    $horizontalPointer = & $alignmentOf $horizontalBranch '_dividerPointerTarget' 'Vertical'
    $horizontalVisual = & $alignmentOf $horizontalBranch '_dividerVisual' 'Vertical'
    if ($horizontalHit -ne 'Top')
    {
        throw "Horizontal divider hit target must use leading-edge Top alignment, found '$horizontalHit'."
    }
    if ($horizontalPointer -ne $horizontalHit -or $horizontalVisual -ne $horizontalHit)
    {
        throw "Horizontal divider visuals must share the hit target's leading-edge alignment (hit '$horizontalHit', pointer '$horizontalPointer', visible '$horizontalVisual')."
    }

    # The visible line stays centered on the split position by subtracting
    # half of its own thickness inside the leading-edge margin.
    if (-not [regex]::IsMatch(
            $verticalBranch,
            '_dividerVisual\s*\.\s*Margin\s*\(\s*ThicknessHelper::FromLengths\s*\(\s*dividerPosition\s*-\s*\(\s*visibleThickness\s*/\s*2\.0\s*\)'))
    {
        throw 'Vertical visible divider margin is not centered on dividerPosition by half its thickness.'
    }
    if (-not [regex]::IsMatch(
            $horizontalBranch,
            '_dividerVisual\s*\.\s*Margin\s*\(\s*ThicknessHelper::FromLengths\s*\(\s*0\s*,\s*dividerPosition\s*-\s*\(\s*visibleThickness\s*/\s*2\.0\s*\)'))
    {
        throw 'Horizontal visible divider margin is not centered on dividerPosition by half its thickness.'
    }

    # The primary-axis Center alignment that produced the v1.1.1 ghost line
    # must not silently return on any divider element.
    foreach ($element in @('_dividerVisual', '_dividerPointerTarget', '_dividerHitTarget'))
    {
        foreach ($axis in @('Horizontal', 'Vertical'))
        {
            if ([regex]::IsMatch(
                    $placement,
                    [string]::Format('{0}\s*\.\s*{1}Alignment\s*\(\s*{1}Alignment::Center', $element, $axis)))
            {
                throw "Divider placement re-introduced the $axis Center alignment on $element."
            }
        }
    }

    # The pointer target must stay wider than the visible line so the drag
    # acquisition area keeps surrounding the rendered divider.
    $visibleTokenMatch = [regex]::Match($tokens, 'DividerVisibleThickness\s*\{\s*([0-9.]+)')
    $hitTokenMatch = [regex]::Match($tokens, 'DividerPointerHitThickness\s*\{\s*([0-9.]+)')
    if (-not $visibleTokenMatch.Success -or -not $hitTokenMatch.Success)
    {
        throw 'Divider thickness tokens could not be parsed.'
    }
    if ([double]$hitTokenMatch.Groups[1].Value -le [double]$visibleTokenMatch.Groups[1].Value)
    {
        throw 'Divider pointer target must remain wider than the visible line.'
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

    Write-Host 'PASS: pane resize snapping, cancellation, DPI, history, and divider placement source boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Pane resize tests failed: $($_.Exception.Message)"
    exit 1
}
