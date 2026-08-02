# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

function Read-Source {
    param([Parameter(Mandatory)][string]$Path)

    return Get-Content -LiteralPath (Join-Path $repositoryRoot $Path) -Raw
}

function Assert-Contains {
    param(
        [Parameter(Mandatory)][string]$Content,
        [Parameter(Mandatory)][string[]]$Values,
        [Parameter(Mandatory)][string]$Failure
    )

    foreach ($value in $Values) {
        if (-not $Content.Contains($value)) {
            throw "$Failure Missing '$value'."
        }
    }
}

$modelHeader = Read-Source 'src\winterm\CommandTimeline\CommandTimelineModel.h'
$modelSource = Read-Source 'src\winterm\CommandTimeline\CommandTimelineModel.cpp'
$controlCoreHeader = Read-Source 'src\cascadia\TerminalControl\ControlCore.h'
$controlCoreSource = Read-Source 'src\cascadia\TerminalControl\ControlCore.cpp'
$termControlHeader = Read-Source 'src\cascadia\TerminalControl\TermControl.h'
$termControlSource = Read-Source 'src\cascadia\TerminalControl\TermControl.cpp'
$termControlXaml = Read-Source 'src\cascadia\TerminalControl\TermControl.xaml'
$defaults = Read-Source 'src\cascadia\TerminalSettingsModel\defaults.json'
$actions = Read-Source 'src\cascadia\TerminalSettingsModel\AllShortcutActions.h'
$actionSerialization = Read-Source 'src\cascadia\TerminalSettingsModel\ActionAndArgs.cpp'
$actionHandler = Read-Source 'src\cascadia\TerminalApp\AppActionHandlers.cpp'
$controlTests = Read-Source 'src\cascadia\UnitTests_Control\CommandTimelineTests.cpp'
$settingsTests = Read-Source 'src\cascadia\UnitTests_SettingsModel\KeyBindingsTests.cpp'
$resources = Read-Source 'src\cascadia\TerminalControl\Resources\en-US\Resources.resw'

Assert-Contains -Content $modelHeader -Values @(
    'class CommandTimelineNavigationModel final',
    'CommandTimelinePresentationSnapshot',
    'CommandTimelineVisibleEntry',
    'ApplyWheelDelta',
    'SettleWheel'
) -Failure 'The pure Command Timeline navigation/presentation model is incomplete.'
Assert-Contains -Content $modelSource -Values @(
    '_wheelDeltaRemainder',
    'NavigationAction::PageFirst',
    'NavigationAction::PageLast',
    '_findNearestCommand',
    '_effectiveResult'
) -Failure 'Command Timeline navigation semantics are incomplete.'
if ($modelHeader.Contains('commandOutput') -or $modelHeader.Contains('cachedOutput')) {
    throw 'The visible Command Timeline model must never contain command output.'
}

Assert-Contains -Content $controlCoreHeader -Values @(
    'CommandTimelineNavigationModel _commandTimelineNavigation',
    'OpenCommandTimeline',
    'RefreshCommandTimeline',
    'ScrollCommandTimeline',
    'CloseCommandTimelineOverlay'
) -Failure 'ControlCore does not own the pane-local Timeline navigation state.'
Assert-Contains -Content $controlCoreSource -Values @(
    '_commandTimelineIndex->Entries()',
    '_commandTimelineNavigation.Reconcile',
    'CommandTimelineChanged.raise',
    '_commandTimelineNavigation.Close()'
) -Failure 'ControlCore does not project Phase 1 data incrementally or clean it up.'

$overlayStart = $termControlXaml.IndexOf('<Border x:Name="CommandTimelineOverlay"', [StringComparison]::Ordinal)
$rendererNoticeStart = $termControlXaml.IndexOf('<Grid x:Name="RendererFailedNotice"', [StringComparison]::Ordinal)
if ($overlayStart -lt 0 -or $rendererNoticeStart -le $overlayStart) {
    throw 'The Command Timeline overlay is not a top-level RootGrid overlay.'
}
$overlayXaml = $termControlXaml.Substring($overlayStart, $rendererNoticeStart - $overlayStart)
Assert-Contains -Content $overlayXaml -Values @(
    'x:Name="CommandTimelineHandle"',
    'Margin="8,0,0,0"',
    'x:Name="CommandTimelineList"',
    'SelectionMode="Single"'
) -Failure 'The overlay handle, bounded list, or selection presentation is incomplete.'
if ($overlayXaml.Contains('SwapChainPanel') -or $overlayXaml.Contains('ColumnDefinition') -or $overlayXaml.Contains('Storyboard')) {
    throw 'The Timeline overlay must not resize the terminal or add an independent animation loop.'
}
if (-not $overlayXaml.Contains('{ThemeResource')) {
    throw 'The Timeline overlay must use High Contrast-aware theme resources.'
}

Assert-Contains -Content $termControlHeader -Values @(
    'SafeDispatcherTimer _commandTimelineWheelSettleTimer',
    'std::shared_ptr<ThrottledFunc<>> _updateCommandTimeline',
    'CommandTimelineChanged_revoker',
    '_commandTimelineConsumedKeys'
) -Failure 'Timeline timer, event, or key cleanup ownership is incomplete.'
Assert-Contains -Content $termControlSource -Values @(
    'GetTSFHandle().HasActiveComposition()',
    '_tryHandleCommandTimelineKey(vkey, modifiers, keyDown)',
    '_tryHandleCommandTimelineWheel(point.Position(), delta)',
    'TextTrimming::CharacterEllipsis',
    '_commandTimelineWheelSettleTimer.Stop()',
    'CommandTimelineList().Items().Clear()',
    'Focus(FocusState::Programmatic)'
) -Failure 'Timeline input isolation, IME precedence, snapping, or close cleanup is incomplete.'
if ($termControlSource.IndexOf('_TryHandleKeyBinding(vkey, scanCode, modifiers)', [StringComparison]::Ordinal) -gt
    $termControlSource.IndexOf('_tryHandleCommandTimelineKey(vkey, modifiers, keyDown)', [StringComparison]::Ordinal)) {
    throw 'User-defined key bindings must retain precedence over bare Timeline navigation.'
}

Assert-Contains -Content $actions -Values @('ON_ALL_ACTIONS(ToggleCommandTimeline)') -Failure 'The Timeline shortcut action is missing.'
Assert-Contains -Content $actionSerialization -Values @('ToggleCommandTimelineKey{ "toggleCommandTimeline" }') -Failure 'The Timeline shortcut is not serializable.'
Assert-Contains -Content $actionHandler -Values @(
    '_HandleToggleCommandTimeline',
    'GetActiveTerminalControl()',
    'control.ToggleCommandTimeline()'
) -Failure 'The Timeline action is not routed to the focused pane.'

$requiredBindings = [ordered]@{
    'ctrl+tab'     = 'winTerm.ToggleCommandTimeline'
    'ctrl+t'       = 'Terminal.NextTab'
    'ctrl+shift+t' = 'Terminal.PrevTab'
    'ctrl+alt+t'   = 'Terminal.OpenNewTab'
}
foreach ($binding in $requiredBindings.GetEnumerator()) {
    $expected = '{ "keys": "' + $binding.Key + '", "id": "' + $binding.Value + '" }'
    if (-not $defaults.Contains($expected)) {
        throw "Required default shortcut is missing: $($binding.Key) -> $($binding.Value)."
    }
    $count = ([regex]::Matches($defaults, '"keys"\s*:\s*"' + [regex]::Escape($binding.Key) + '"', 'IgnoreCase')).Count
    if ($count -ne 1) {
        throw "Default shortcut '$($binding.Key)' must occur exactly once, found $count."
    }
}
if ($defaults.Contains('{ "keys": "ctrl+shift+tab"')) {
    throw 'The replaced Ctrl+Shift+Tab default binding is still present.'
}

Assert-Contains -Content $resources -Values @(
    'CommandTimelineHandle.[using:Windows.UI.Xaml.Automation]AutomationProperties.Name',
    '<value>Open command timeline</value>',
    'CommandTimelineStatusUnknown'
) -Failure 'Timeline accessibility names or status text are missing.'

Assert-Contains -Content $controlTests -Values @(
    'NavigationPageEdgesMoveOneInOneOut',
    'NavigationWheelAccumulatesReversesAndSettles',
    'NavigationStateIsPaneLocalAndRepeatedCloseIsSafe',
    'ColdBootstrapScansOnceAndWarmAccessDoesNotRescan'
) -Failure 'Deterministic Timeline navigation, pane isolation, cleanup, or warm-access tests are missing.'
Assert-Contains -Content $settingsTests -Values @(
    'CommandTimelineDefaultShortcutsAndUserOverride',
    'ShortcutAction::ToggleCommandTimeline',
    'OriginTag::User'
) -Failure 'Settings Model shortcut and user-precedence tests are missing.'

[xml](Read-Source 'src\cascadia\TerminalControl\TermControl.xaml') | Out-Null
[xml](Read-Source 'src\cascadia\TerminalControl\Resources\en-US\Resources.resw') | Out-Null
Write-Host 'PASS: Command Timeline Phase 2 source, input, accessibility, privacy, and lifecycle boundaries' -ForegroundColor Green
