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

Assert-Contains -Content $modelHeader -Values @(
    'class CommandTimelineActionModel final',
    'enum class CommandActionKind',
    'CommandActionKind::LoadIntoInput',
    'struct CommandLoadPolicy',
    'NotifyExecutionStarted',
    'IsCurrentGeneration',
    'ReconcileLoadedInput'
) -Failure 'The pure Command Timeline entry-action model is incomplete.'
Assert-Contains -Content $modelSource -Values @(
    'CommandActionStatus::MultilineUnsafe',
    'CommandActionStatus::ConfirmationRequired',
    'CommandActionStatus::OutputUnavailable',
    '++viewState.executionGeneration',
    'viewState.loadedCommandId.reset()'
) -Failure 'Command Timeline load, confirmation, or execution-generation semantics are incomplete.'

Assert-Contains -Content $modelHeader -Values @(
    'MaxCommandTimelineQueryLength = 256',
    'DefaultCommandTimelineHistoryLimit = 500',
    'MinCommandTimelineHistoryLimit = 50',
    'MaxCommandTimelineHistoryLimit = 5000',
    'NormalizeCommandTimelineQuery',
    'CommandTimelineQueryMatches',
    'ClampCommandTimelineHistoryLimit',
    'enum class CommandTimelineEmptyState',
    'SetHistoryLimit'
) -Failure 'Phase 4 search bounds, history limit, or degradation states are incomplete.'
Assert-Contains -Content $modelSource -Values @(
    'CommandTimelineEmptyState::NoMatchingCommands',
    'CommandTimelineEmptyState::ShellUnsupported',
    'CommandTimelineEmptyState::WaitingForShell',
    '_rebuildFilter',
    '_nearestPosition',
    '_applyHistoryLimit'
) -Failure 'Phase 4 filtered projection or eviction semantics are incomplete.'

# Search must be literal and case-insensitive over command text only. A regex
# engine, a fuzzy matcher, or an output field would each break the stated
# privacy and performance boundaries. Compare against code only: the model
# documents why these are excluded, and that prose must not trip the guard.
$modelCode = [regex]::Replace($modelSource, '//.*', '') + [regex]::Replace($modelHeader, '//.*', '')
foreach ($forbidden in @('std::regex', 'std::wregex', 'regex_search', 'fuzzy', 'Fuzzy')) {
    if ($modelCode.Contains($forbidden)) {
        throw "Command Timeline search must be literal. Found '$forbidden'."
    }
}
$matchStart = $modelSource.IndexOf('bool CommandTimelineQueryMatches', [StringComparison]::Ordinal)
if ($matchStart -lt 0) {
    throw 'CommandTimelineQueryMatches is missing.'
}
$matchBody = $modelSource.Substring($matchStart, $modelSource.IndexOf("`n    }", $matchStart, [StringComparison]::Ordinal) - $matchStart)
if (-not $matchBody.Contains('std::search') -or -not $matchBody.Contains('towlower')) {
    throw 'Command Timeline search must be a case-insensitive literal substring search.'
}

$filterStart = $modelSource.IndexOf('void CommandTimelineNavigationModel::_rebuildFilter', [StringComparison]::Ordinal)
$filterBody = $modelSource.Substring($filterStart, $modelSource.IndexOf("`n    }", $filterStart, [StringComparison]::Ordinal) - $filterStart)
if (-not $filterBody.Contains('cachedCommandText')) {
    throw 'Command Timeline filtering must only read the bounded cached command text.'
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

Assert-Contains -Content $controlCoreHeader -Values @(
    'CommandTimelineActionModel _commandTimelineActions',
    'PrepareCommandTimelineAction',
    'LoadCommandTimelineCommand',
    'ResolveCommandTimelineOutput',
    'JumpToCommandTimelineOutput',
    'CopyCommandTimelineText'
) -Failure 'ControlCore does not expose the pane-local Timeline entry actions.'
Assert-Contains -Content $controlCoreSource -Values @(
    'FilterStringForPaste(request.commandText, ControlCodes)',
    '_commandTimelineActions.NotifyLoaded',
    '_commandTimelineActions.NotifyExecutionStarted',
    '_commandTimelineActions.ReconcileLoadedInput',
    '_terminal->ResolveCommandTimelineOutput(request.nativeMarkId)'
) -Failure 'Timeline load, generation tracking, or on-demand output resolution is incomplete.'

# A Timeline load must never submit the command. Requesting the
# CarriageReturnNewline paste filter, or appending a carriage return to the
# payload, would turn a load into an execution.
$loadStart = $controlCoreSource.IndexOf('bool ControlCore::LoadCommandTimelineCommand', [StringComparison]::Ordinal)
if ($loadStart -lt 0) {
    throw 'ControlCore::LoadCommandTimelineCommand is missing.'
}
$loadEnd = $controlCoreSource.IndexOf("`n    }", $loadStart, [StringComparison]::Ordinal)
$loadBody = $controlCoreSource.Substring($loadStart, $loadEnd - $loadStart)
# Compare against code only. The body documents why these constructs are
# excluded, so the prose must not trip the guard.
$loadCode = [regex]::Replace($loadBody, '//.*', '')
foreach ($forbidden in @('CarriageReturnNewline', 'append(L"\r")', "append(L'\r')", 'L"\r\n"')) {
    if ($loadCode.Contains($forbidden)) {
        throw "The Timeline load path must never submit the command. Found '$forbidden'."
    }
}
if ($loadCode.Contains('clipboard::') -or $loadCode.Contains('Clipboard::GetContent')) {
    throw 'The Timeline load path must never read the Windows clipboard.'
}
if (-not $loadCode.Contains('SendInput(payload)')) {
    throw 'The Timeline load must target this pane connection directly.'
}

$overlayStart = $termControlXaml.IndexOf('<Border x:Name="CommandTimelineOverlay"', [StringComparison]::Ordinal)
$rendererNoticeStart = $termControlXaml.IndexOf('<Grid x:Name="RendererFailedNotice"', [StringComparison]::Ordinal)
if ($overlayStart -lt 0 -or $rendererNoticeStart -le $overlayStart) {
    throw 'The Command Timeline overlay is not a top-level RootGrid overlay.'
}
$overlayXaml = $termControlXaml.Substring($overlayStart, $rendererNoticeStart - $overlayStart)
Assert-Contains -Content $overlayXaml -Values @(
    'x:Name="CommandTimelineHandle"',
    'Width="6"',
    'PointerEntered="_CommandTimelineHandlePointerEntered"',
    'PointerExited="_CommandTimelineHandlePointerExited"',
    'x:Name="CommandTimelineList"',
    'SelectionMode="Single"'
) -Failure 'The overlay auto-hiding handle, bounded list, or selection presentation is incomplete.'
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
    '_commandTimelineOpen && !_isPointOverCommandTimeline(point.Position())',
    '_updateCommandTimelineHandleVisual()',
    'TextTrimming::CharacterEllipsis',
    '_commandTimelineWheelSettleTimer.Stop()',
    'CommandTimelineList().Items().Clear()',
    'Focus(FocusState::Programmatic)'
) -Failure 'Timeline input isolation, IME precedence, light dismiss, or close cleanup is incomplete.'
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

Assert-Contains -Content $termControlSource -Values @(
    '_loadCommandTimelineSelection()',
    '_jumpToCommandTimelineOutput()',
    '_copyCommandTimelineCommand()',
    '_copyCommandTimelineOutput()',
    'CommandActionKind::LoadIntoInput',
    'CommandActionStatus::MultilineUnsafe',
    '_clearCommandTimelinePendingLoad()',
    'item.ContextRequested'
) -Failure 'Timeline load, jump, copy, or context-menu routing is incomplete.'
Assert-Contains -Content $resources -Values @(
    'CommandTimelineCopyCommand',
    'CommandTimelineCopyOutput',
    'CommandTimelineJumpToOutput',
    'CommandTimelineOutputUnavailable',
    'CommandTimelineStatusUnknownDetail',
    'CommandTimelineMultilineBlocked',
    'CommandTimelineConfirmLoad'
) -Failure 'Timeline entry-action localized strings are missing.'

# Phase 4: search box, settings, and degradation states.
Assert-Contains -Content $resources -Values @(
    'CommandTimelineSearchBox.[using:Windows.UI.Xaml.Automation]AutomationProperties.Name',
    'CommandTimelineNoMatchingCommands',
    'CommandTimelineWaitingForShell'
) -Failure 'Timeline search accessibility name or empty-state strings are missing.'
Assert-Contains -Content $overlayXaml -Values @(
    'x:Name="CommandTimelineSearchBox"',
    'MaxLength="256"',
    'KeyDown="_CommandTimelineSearchKeyDown"',
    'TextChanged="_CommandTimelineSearchTextChanged"'
) -Failure 'The Timeline filter box is missing or unbounded.'
Assert-Contains -Content $termControlSource -Values @(
    'FilterCommandTimeline',
    'NormalizeCommandTimelineQuery',
    '_focusCommandTimelineSearch()',
    'CommandTimelineSearchBox().Text({})',
    'CommandTimelineEmptyState::NoMatchingCommands',
    'presentation.filteredEntryCount',
    '_applyCommandTimelineEnabledSetting'
) -Failure 'Timeline filtering, focus routing, cleanup, or enabled-setting handling is incomplete.'

$settingsMacros = Read-Source 'src\cascadia\TerminalSettingsModel\MTSMSettings.h'
Assert-Contains -Content $settingsMacros -Values @(
    'X(bool, CommandTimelineEnabled, "commandTimeline.enabled", true)',
    'X(int32_t, CommandTimelineHistoryLimit, "commandTimeline.historyLimit", 500)'
) -Failure 'The public Command Timeline settings are not declared with their documented defaults.'
Assert-Contains -Content $defaults -Values @(
    '"commandTimeline.enabled": true',
    '"commandTimeline.historyLimit": 500'
) -Failure 'The Command Timeline defaults are missing from defaults.json.'

$schema = Read-Source 'doc\cascadia\profiles.schema.json'
Assert-Contains -Content $schema -Values @(
    '"commandTimeline.enabled"',
    '"commandTimeline.historyLimit"',
    '"maximum": 5000',
    '"minimum": 50'
) -Failure 'The Command Timeline settings schema is missing its type, default, or range.'

$settingsUi = Read-Source 'src\cascadia\TerminalSettingsEditor\GlobalAppearance.xaml'
Assert-Contains -Content $settingsUi -Values @(
    'x:Uid="Globals_CommandTimelineHeader"',
    'ViewModel.CommandTimelineEnabled',
    'ViewModel.CommandTimelineHistoryLimit',
    'Maximum="5000"',
    'Minimum="50"'
) -Failure 'The Settings UI does not expose the Command Timeline settings within their range.'

Assert-Contains -Content $controlTests -Values @(
    'NavigationPageEdgesMoveOneInOneOut',
    'NavigationWheelAccumulatesReversesAndSettles',
    'NavigationStateIsPaneLocalAndRepeatedCloseIsSafe',
    'ColdBootstrapScansOnceAndWarmAccessDoesNotRescan',
    'ActionLoadPreparesSelectedCommandWithoutExecuting',
    'ActionLoadRefusesMultilineWithoutBracketedPaste',
    'ActionLateCompletionIsDetectedAfterExecutionStart',
    'ActionLoadedInputIsReleasedOnEviction',
    'ActionCopyResolvesStableCommandIdNotRowIndex',
    'ActionOutputRequiresLiveNativeRangeAndIsNeverCached',
    'SearchMatchesLiterallyAndCaseInsensitively',
    'SearchQueryTruncationIsSurrogateSafe',
    'SearchKeepsStableSelectionAcrossQueryChanges',
    'SearchSelectsNearestSurvivingMatch',
    'SearchNavigationAndWheelWalkFilteredProjection',
    'SearchNewCommandFollowsLatestOnlyWhenMatching',
    'ShellDegradationStatesAreDistinct',
    'HistoryLimitEvictsOldestFirstAndNeverResurrects',
    'HistoryLimitClampsToSupportedRange',
    'SearchCloseReleasesQueryAndProjection',
    'SearchStressAtMaximumHistoryLimit'
) -Failure 'Deterministic Timeline navigation, action, search, eviction, pane isolation, cleanup, or warm-access tests are missing.'

$commandTimelineSettingsTests = Read-Source 'src\cascadia\UnitTests_SettingsModel\WinTermCommandTimelineTests.cpp'
Assert-Contains -Content $commandTimelineSettingsTests -Values @(
    'CommandTimelineSettingsUseStableDefaults',
    'CommandTimelineSettingsRoundTripThroughJson',
    'CommandTimelineHistoryLimitIsClampedAtRuntime',
    'CommandTimelineSettingsTolerateOutOfRangeAndOddValues'
) -Failure 'Command Timeline settings defaults, round-trip, range, and invalid-value tests are missing.'
Assert-Contains -Content $settingsTests -Values @(
    'CommandTimelineDefaultShortcutsAndUserOverride',
    'ShortcutAction::ToggleCommandTimeline',
    'OriginTag::User'
) -Failure 'Settings Model shortcut and user-precedence tests are missing.'

[xml](Read-Source 'src\cascadia\TerminalControl\TermControl.xaml') | Out-Null
[xml](Read-Source 'src\cascadia\TerminalControl\Resources\en-US\Resources.resw') | Out-Null
Write-Host 'PASS: Command Timeline Phase 4 source, search, settings, input, action, accessibility, privacy, and lifecycle boundaries' -ForegroundColor Green
