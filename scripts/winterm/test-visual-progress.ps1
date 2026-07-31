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
        'src\winterm\VisualProgress\VisualProgressModel.h',
        'src\cascadia\UnitTests_SettingsModel\WinTermVisualProgressTests.cpp',
        'src\cascadia\TerminalApp\Pane.cpp',
        'src\cascadia\TerminalApp\Pane.h',
        'src\cascadia\TerminalApp\App.xaml',
        'src\cascadia\TerminalSettingsModel\MTSMSettings.h',
        'src\cascadia\TerminalSettingsModel\GlobalAppSettings.idl',
        'src\terminal\adapter\adaptDispatch.cpp',
        'docs\development\visual-progress-phase1.md',
        'scripts\winterm\invoke-visual-progress-smoke.ps1'
    )
    foreach ($relativePath in $requiredFiles)
    {
        if (-not (Test-Path -LiteralPath (Join-Path $root $relativePath) -PathType Leaf))
        {
            throw "Visual Progress boundary '$relativePath' is missing."
        }
    }

    $model = Get-Content -LiteralPath (Join-Path $root $requiredFiles[0]) -Raw
    foreach ($required in @(
        'ProgressMode',
        'ProgressStatus',
        'ProgressSource',
        'ProgressStateMachine',
        'ProgressUpdateMailbox',
        'std::min<uint64_t>(value, 100)',
        'SamePresentation',
        'ShellLifecycleState::CommandStart',
        'ShellLifecycleState::CommandFinished',
        'emergencyOverride != L"1"'
    ))
    {
        Assert-Contains $model $required 'Normalized progress model'
    }
    if ($model -match '(?i)timer|animation|particle|spark|glow|bloom|scrollback|command text|environment variables')
    {
        throw 'The normalized Visual Progress core contains a Phase 2 effect, polling primitive, or sensitive data field.'
    }

    $pane = Get-Content -LiteralPath (Join-Path $root $requiredFiles[2]) -Raw
    foreach ($required in @(
        'WINTERM_DISABLE_VISUAL_PROGRESS',
        '_content.TaskbarState()',
        '_content.TaskbarProgress()',
        'ShellIntegrationChanged',
        '_visualProgressOverlay.Height(6.0)',
        'ThicknessHelper::FromLengths(10.0, 0.0, 10.0, 8.0)',
        '_visualProgressOverlay.IsHitTestVisible(false)',
        'Controls::Grid::SetRow(_visualProgressOverlay, 1)',
        'std::weak_ptr<Pane>',
        'CoreDispatcherPriority::Low',
        '_visualProgressMailbox.TakeLatest()',
        '_paneTaskbarProgressChangedRevoker.revoke()',
        '_paneShellIntegrationChangedRevoker.revoke()',
        '_visualProgressState.Reset()',
        '_visualProgressMailbox.Close()',
        'LOG_CAUGHT_EXCEPTION()',
        '_DestroyVisualProgressOverlay()'
    ))
    {
        Assert-Contains $pane $required 'Per-pane Visual Progress integration'
    }
    if ($pane -match '(?i)DispatcherTimer|CompositionAnimation|VisualProgress.*Storyboard')
    {
        throw 'The Phase 1 pane overlay must remain static and timer-free.'
    }

    $settings = Get-Content -LiteralPath (Join-Path $root $requiredFiles[5]) -Raw
    Assert-Contains $settings 'VisualProgressEnabled, "visualProgress.enabled", false' 'Visual Progress setting'
    $settingsIdl = Get-Content -LiteralPath (Join-Path $root $requiredFiles[6]) -Raw
    Assert-Contains $settingsIdl 'INHERITABLE_SETTING(Boolean, VisualProgressEnabled)' 'Visual Progress setting projection'

    $dispatch = Get-Content -LiteralPath (Join-Path $root $requiredFiles[7]) -Raw
    foreach ($required in @(
        'ShellIntegrationMarkKind::Prompt',
        'ShellIntegrationMarkKind::CommandStart',
        'ShellIntegrationMarkKind::CommandExecuted',
        'ShellIntegrationMarkKind::CommandFinished'
    ))
    {
        Assert-Contains $dispatch $required 'Semantic shell lifecycle boundary'
    }

    $tests = Get-Content -LiteralPath (Join-Path $root $requiredFiles[1]) -Raw
    foreach ($required in @(
        'MapEveryTaskbarState',
        'ClampDeterminateValues',
        'SuppressDuplicateState',
        'EmergencyOverridePrecedesSetting',
        'MultiplePanesRemainIndependent',
        'CloseAndDetachCleanupStopsUpdates',
        'SplitOrDetachResetClearsReusableState',
        'FeatureReloadDisablesAndReenablesCleanly',
        'MailboxCoalescesRapidUpdatesAndReleasesOnClose',
        'SettingSerializesAndMissingSettingDefaultsOff'
    ))
    {
        Assert-Contains $tests $required 'Visual Progress compiled test coverage'
    }

    $project = Get-Content -LiteralPath (Join-Path $root 'src\cascadia\UnitTests_SettingsModel\SettingsModel.UnitTests.vcxproj') -Raw
    Assert-Contains $project '<ClCompile Include="WinTermVisualProgressTests.cpp" />' 'Compiled Visual Progress test registration'

    $fixture = Get-Content -LiteralPath (Join-Path $root $requiredFiles[9]) -Raw
    foreach ($required in @('9;4;0', '9;4;1;50', '9;4;3', '9;4;4;65', '9;4;2;65', '133;B', '133;D;0'))
    {
        Assert-Contains $fixture $required 'Manual Visual Progress fixture'
    }

    $testBinary = Join-Path $root "bin\$Platform\$Configuration\UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll"
    if ($SourceOnly)
    {
        Write-Host 'SKIP: compiled Visual Progress tests are handled by the parent suite.' -ForegroundColor Yellow
    }
    elseif (Test-Path -LiteralPath $testBinary -PathType Leaf)
    {
        & (Join-Path $PSScriptRoot 'test.ps1') -Suite Relevant -Configuration $Configuration -Platform $Platform
        if (-not $?)
        {
            throw 'Compiled Visual Progress tests failed.'
        }
    }
    elseif ($RequireCompiled)
    {
        throw "Compiled Settings Model tests were not found at '$testBinary'."
    }
    else
    {
        Write-Host 'SKIP: compiled Visual Progress tests are unavailable.' -ForegroundColor Yellow
    }

    Write-Host 'PASS: Visual Progress Phase 1 source, lifecycle, and safety boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Visual Progress tests failed: $($_.Exception.Message)"
    exit 1
}
