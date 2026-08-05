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

function Assert-NotContains
{
    param(
        [Parameter(Mandatory)]
        [string]$Content,

        [Parameter(Mandatory)]
        [string]$Value,

        [Parameter(Mandatory)]
        [string]$Description
    )

    if ($Content.Contains($Value))
    {
        throw "$Description unexpectedly contains '$Value'."
    }
}

function Assert-Matches
{
    param(
        [Parameter(Mandatory)]
        [string]$Content,

        [Parameter(Mandatory)]
        [string]$Pattern,

        [Parameter(Mandatory)]
        [string]$Description
    )

    if ($Content -notmatch $Pattern)
    {
        throw "$Description does not match '$Pattern'."
    }
}

function Assert-NotMatches
{
    param(
        [Parameter(Mandatory)]
        [string]$Content,

        [Parameter(Mandatory)]
        [string]$Pattern,

        [Parameter(Mandatory)]
        [string]$Description
    )

    if ($Content -match $Pattern)
    {
        throw "$Description matches forbidden pattern '$Pattern'."
    }
}

function Assert-Before
{
    param(
        [Parameter(Mandatory)]
        [string]$Content,

        [Parameter(Mandatory)]
        [string]$Before,

        [Parameter(Mandatory)]
        [string]$After,

        [Parameter(Mandatory)]
        [string]$Description
    )

    $beforeIndex = $Content.IndexOf($Before, [System.StringComparison]::Ordinal)
    $afterIndex = $Content.IndexOf($After, [System.StringComparison]::Ordinal)
    if ($beforeIndex -lt 0 -or $afterIndex -lt 0 -or $beforeIndex -ge $afterIndex)
    {
        throw "$Description must place '$Before' before '$After'."
    }
}

function Get-RequiredContent
{
    param(
        [Parameter(Mandatory)]
        [string]$Root,

        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    $path = Join-Path $Root $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf))
    {
        throw "Visual Progress Phase 3 boundary '$RelativePath' is missing."
    }
    return Get-Content -LiteralPath $path -Raw
}

try
{
    $root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $paths = [ordered]@{
        Model = 'src\winterm\VisualProgress\VisualProgressModel.h'
        Constants = 'src\winterm\VisualProgress\RainbowArcVisualConstants.h'
        RenderModel = 'src\winterm\VisualProgress\VisualProgressRenderModel.h'
        Governor = 'src\winterm\VisualProgress\VisualProgressPerformanceGovernor.h'
        Accessibility = 'src\winterm\VisualProgress\VisualProgressAccessibility.h'
        SamplerState = 'src\winterm\VisualProgress\VisualProgressSamplerState.h'
        Renderer = 'src\winterm\VisualProgress\RainbowArcRenderer.h'
        Recognition = 'src\winterm\VisualProgress\ProgressRecognition.h'
        Coordinator = 'src\cascadia\TerminalApp\VisualProgressWindowCoordinator.h'
        SafeDispatcherTimer = 'src\cascadia\WinRTUtils\inc\SafeDispatcherTimer.h'
        Tests = 'src\cascadia\UnitTests_SettingsModel\WinTermVisualProgressTests.cpp'
        TestProject = 'src\cascadia\UnitTests_SettingsModel\SettingsModel.UnitTests.vcxproj'
        PaneCpp = 'src\cascadia\TerminalApp\Pane.cpp'
        PaneH = 'src\cascadia\TerminalApp\Pane.h'
        TabCpp = 'src\cascadia\TerminalApp\Tab.cpp'
        TabH = 'src\cascadia\TerminalApp\Tab.h'
        TabManagement = 'src\cascadia\TerminalApp\TabManagement.cpp'
        TerminalPageCpp = 'src\cascadia\TerminalApp\TerminalPage.cpp'
        TerminalPageH = 'src\cascadia\TerminalApp\TerminalPage.h'
        TerminalAppProject = 'src\cascadia\TerminalApp\TerminalAppLib.vcxproj'
        TerminalAppFilters = 'src\cascadia\TerminalApp\TerminalAppLib.vcxproj.filters'
        AppXaml = 'src\cascadia\TerminalApp\App.xaml'
        AppResources = 'src\cascadia\TerminalApp\Resources\en-US\Resources.resw'
        TerminalPaneContentCpp = 'src\cascadia\TerminalApp\TerminalPaneContent.cpp'
        TerminalPaneContentH = 'src\cascadia\TerminalApp\TerminalPaneContent.h'
        TerminalPaneContentIdl = 'src\cascadia\TerminalApp\TerminalPaneContent.idl'
        ControlCoreCpp = 'src\cascadia\TerminalControl\ControlCore.cpp'
        ControlCoreH = 'src\cascadia\TerminalControl\ControlCore.h'
        ControlCoreIdl = 'src\cascadia\TerminalControl\ControlCore.idl'
        ICoreStateIdl = 'src\cascadia\TerminalControl\ICoreState.idl'
        TermControlCpp = 'src\cascadia\TerminalControl\TermControl.cpp'
        TermControlH = 'src\cascadia\TerminalControl\TermControl.h'
        TermControlIdl = 'src\cascadia\TerminalControl\TermControl.idl'
        TerminalCpp = 'src\cascadia\TerminalCore\Terminal.cpp'
        TerminalHpp = 'src\cascadia\TerminalCore\Terminal.hpp'
        Settings = 'src\cascadia\TerminalSettingsModel\MTSMSettings.h'
        SettingsIdl = 'src\cascadia\TerminalSettingsModel\GlobalAppSettings.idl'
        SettingsSerialization = 'src\cascadia\TerminalSettingsModel\TerminalSettingsSerializationHelpers.h'
        EnumMappingsIdl = 'src\cascadia\TerminalSettingsModel\EnumMappings.idl'
        EnumMappingsH = 'src\cascadia\TerminalSettingsModel\EnumMappings.h'
        EnumMappingsCpp = 'src\cascadia\TerminalSettingsModel\EnumMappings.cpp'
        GlobalAppearanceXaml = 'src\cascadia\TerminalSettingsEditor\GlobalAppearance.xaml'
        GlobalAppearanceViewModelIdl = 'src\cascadia\TerminalSettingsEditor\GlobalAppearanceViewModel.idl'
        GlobalAppearanceViewModelH = 'src\cascadia\TerminalSettingsEditor\GlobalAppearanceViewModel.h'
        GlobalAppearanceViewModelCpp = 'src\cascadia\TerminalSettingsEditor\GlobalAppearanceViewModel.cpp'
        SettingsResources = 'src\cascadia\TerminalSettingsEditor\Resources\en-US\Resources.resw'
        SettingsSchema = 'doc\cascadia\profiles.schema.json'
        SettingsIndexGenerator = 'tools\GenerateSettingsIndex.ps1'
        Dispatch = 'src\terminal\adapter\adaptDispatch.cpp'
        Phase1Doc = 'docs\development\visual-progress-phase1.md'
        Phase2Doc = 'docs\development\visual-progress-phase2.md'
        Changelog = 'CHANGELOG.md'
        Readme = 'README.md'
        Privacy = 'PRIVACY.md'
        CurrentProgress = 'docs\current-progress.md'
        VersionMetadata = 'src\winterm\Branding\version.json'
        ReleaseMetadata = 'src\winterm\Branding\ReleaseMetadata.h'
        PackageManifest = 'src\cascadia\CascadiaPackage\Package-winTerm.appxmanifest'
        HostResource = 'src\cascadia\WindowsTerminal\WindowsTerminal.rc'
        ShimResource = 'src\winterm-tools\winterm-shim\winterm-shim.rc'
        CustomProps = 'custom.props'
        ShellVersion = 'shell\shared\version.json'
        ShellModuleManifest = 'shell\powershell\winTerm.Shell\winTerm.Shell.psd1'
        ShellModule = 'shell\powershell\winTerm.Shell\winTerm.Shell.psm1'
        PackageShellAssets = 'scripts\winterm\package-shell-assets.ps1'
        WorkspaceSerializer = 'src\winterm\Workspaces\Persistence\WorkspaceSerializer.cpp'
        VerifyVersion = 'scripts\winterm\verify-version.ps1'
        Smoke = 'scripts\winterm\invoke-visual-progress-smoke.ps1'
    }

    $source = @{}
    foreach ($entry in $paths.GetEnumerator())
    {
        $source[$entry.Key] = Get-RequiredContent -Root $root -RelativePath $entry.Value
    }

    $model = $source.Model
    foreach ($required in @(
        'ProgressMode',
        'ProgressStatus',
        'ProgressSource',
        'Provider,',
        'ProgressProvider',
        'ProviderConfidence',
        'ProviderProgress',
        'PackProviderProgress',
        'UnpackProviderProgress',
        'ProgressStateMachine',
        'ApplyProvider',
        'ProgressSource::Provider',
        '_providerSnapshot',
        '_fallbackSnapshot',
        'SamePresentation',
        'ProgressUpdateMailbox',
        'std::try_to_lock',
        'std::min<uint64_t>(value, 100)',
        'ShellLifecycleState::CommandStart',
        'ShellLifecycleState::CommandFinished',
        'emergencyOverride != L"1"'
    ))
    {
        Assert-Contains $model $required 'Extended normalized progress model'
    }
    Assert-NotMatches $model 'std::wstring(?!_view)|std::string(?!_view)|winrt::hstring' 'Normalized progress state text-retention boundary'

    foreach ($required in @(
        'uint64_t launchGeneration{}',
        'launchGeneration == other.launchGeneration',
        'std::optional<ProgressSnapshot> ExpireShellLaunch(const uint64_t generation) noexcept',
        'generation != _shellLaunchGeneration || _shellLaunchExpired',
        '_beginShellLaunchScope',
        '_resetShellLaunchScope',
        'HiddenSnapshot(ProgressStatus::Running)'
    ))
    {
        Assert-Contains $model $required 'Bounded one-shot shell launch fallback policy'
    }
    Assert-Matches $model '(?s)case ShellLifecycleState::CommandExecuted:.*?if \(_shellLifecycle == ShellLifecycleState::CommandExecuted\)\s*\{\s*break;\s*\}' 'Idempotent CommandExecuted re-broadcast boundary'

    $providers = @(
        'DockerPull',
        'DockerBuildKit',
        'Pip',
        'Git',
        'Curl',
        'Wget',
        'Npm',
        'Pnpm',
        'Yarn',
        'Nvm',
        'Maven',
        'Gradle',
        'Generic'
    )
    foreach ($provider in $providers)
    {
        Assert-Contains $model $provider "Normalized provider enum ($provider)"
        Assert-Contains $source.Recognition "ProgressProvider::$provider" "Built-in recognition provider ($provider)"
        Assert-Contains $source.Tests "ProgressProvider::$provider" "Compiled provider fixture ($provider)"
    }

    $constants = $source.Constants
    foreach ($required in @(
        'TrackHeight{ 6.0f }',
        'HorizontalInset{ 10.0f }',
        'BottomInset{ 8.0f }',
        'TrackCornerRadius{ TrackHeight / 2.0f }',
        'WhiteCoreWidth{ 2.5f }',
        'WarmCoreWidth{ 5.0f }',
        'HeadTrailWidth{ 8.0f }',
        'InnerGlowWidth{ 12.0f }',
        'OuterBloomWidth{ 26.0f }',
        'OverlayHostHeight{ BottomInset + TrackHeight + OuterBloomHeight }',
        'RainbowCycleDuration{ 2000 }',
        'IndeterminateCycleDuration{ 1800 }',
        'DeterminateInterpolationDuration{ 220 }',
        'RegressionInterpolationDuration{ 240 }',
        'WaitingBreatheDuration{ 1600 }',
        'SuccessSweepDuration{ 320 }',
        'SuccessFadeDuration{ 650 }',
        'ErrorPulseDuration{ 220 }',
        'CancelFadeDuration{ 180 }',
        'MinimumSparkLifetime{ 120 }',
        'MaximumSparkLifetime{ 260 }',
        'SparkPoolCapacityPerPane{ 8 }',
        'SparkCapacityPerWindowOrProcess{ 24 }',
        'LightRunningSolid',
        'LightWaitingSolid',
        'LightSuccessSolid',
        'LightErrorSolid'
    ))
    {
        Assert-Contains $constants $required 'Centralized Rainbow Arc visual constants'
    }
    foreach ($color in @('RainbowRed', 'RainbowOrange', 'RainbowYellow', 'RainbowGreen', 'RainbowCyan', 'RainbowBlue', 'RainbowViolet', 'RainbowMagenta'))
    {
        Assert-Contains $constants $color 'Centralized continuous rainbow palette'
    }

    $renderModel = $source.RenderModel
    foreach ($required in @(
        'enum class RenderTier',
        'Full,',
        'NoSparks,',
        'StaticGradient,',
        'Solid,',
        'Disabled,',
        'NextLowerRenderTier',
        'struct RenderEnvironment',
        'animationsEnabled',
        'highContrast',
        'paneActive',
        'windowVisible',
        'windowFocused',
        'UsesStaticFallback',
        'AllowsContinuousAnimation',
        'using RenderTimestamp = std::chrono::milliseconds',
        'struct RenderTransitionPlan',
        'phaseReset',
        'indeterminateMoving',
        'successSweep',
        'finalSparkBurst',
        'errorPulse',
        'errorWithoutProgress',
        'releaseAfterTransition',
        'class VisualProgressRenderState',
        'class SparkBudget',
        'class SparkPool',
        'std::array<Slot, RainbowArcVisualConstants::SparkPoolCapacityPerPane>',
        'RequiresSparkWork'
    ))
    {
        Assert-Contains $renderModel $required 'Deterministic renderer and resource model'
    }

    $governor = $source.Governor
    foreach ($required in @(
        'enum class PerformanceMode',
        'Automatic,',
        'Full,',
        'Balanced,',
        'Minimal,',
        'struct PerformanceGovernorInputs',
        'struct PerformanceRuntimeEnvironment',
        'struct PerformanceGovernorDecision',
        'MostRestrictiveRenderTier',
        'NextLowerAdaptiveRenderTier',
        'NextHigherRenderTier',
        'class VisualProgressPerformanceGovernor final',
        'MinimumSampleInterval = std::chrono::milliseconds{ 1000 }',
        'UnhealthyDispatchLatency = std::chrono::milliseconds{ 100 }',
        'HealthyDispatchLatency = std::chrono::milliseconds{ 40 }',
        'RecoveryCooldown = std::chrono::seconds{ 10 }',
        'UnhealthySamplesToDegrade = 3',
        'HealthySamplesToRecover = 15',
        'visibleActiveProgressCount >= 4',
        'visibleActiveProgressCount >= 2',
        'case PerformanceMode::Balanced:',
        'return RenderTier::NoSparks;',
        'case PerformanceMode::Minimal:',
        'return RenderTier::StaticGradient;',
        'if (inputs.highContrast)',
        'RenderTier::Solid',
        '!inputs.osAnimationsEnabled',
        '!inputs.applicationAnimationsEnabled',
        'inputs.softwareRendering',
        'inputs.remoteSession',
        'inputs.energySaver',
        '!inputs.effectsFast',
        'ObserveDispatchLatency',
        'ObserveHardFailure',
        'NextLowerAdaptiveRenderTier(_adaptiveTier)',
        'NextLowerRenderTier(_adaptiveTier)',
        'NextHigherRenderTier(_adaptiveTier)',
        'void ResetIdle() noexcept',
        'inputs.visibleActiveProgressCount == 0',
        'decision.shouldSample = decision.present',
        'inputs.windowFocused',
        'inputs.paneActive'
    ))
    {
        Assert-Contains $governor $required 'Pure adaptive performance governor'
    }
    Assert-Matches $governor '(?s)case PerformanceMode::Balanced:.*?return RenderTier::NoSparks;.*?case PerformanceMode::Minimal:.*?return RenderTier::StaticGradient;' 'Balanced and Minimal quality ceilings'
    Assert-Matches $governor '(?s)if \(inputs\.highContrast\).*?RenderTier::Solid' 'High Contrast system-compatible tier cap'
    Assert-Matches $governor '(?s)!inputs\.osAnimationsEnabled\s*\|\|\s*!inputs\.applicationAnimationsEnabled.*?RenderTier::StaticGradient' 'OS and application Reduced Motion tier cap'
    Assert-Matches $governor '(?s)NextLowerAdaptiveRenderTier.*?nextTier == RenderTier::Disabled \? RenderTier::Solid : nextTier' 'Adaptive latency degradation floor'
    Assert-Matches $governor '(?s)if \(latency >= UnhealthyDispatchLatency\).*?NextLowerAdaptiveRenderTier\(_adaptiveTier\)' 'Latency uses the recoverable adaptive degradation path'
    Assert-Matches $governor '(?s)ObserveHardFailure.*?NextLowerRenderTier\(_adaptiveTier\)' 'Hard failures retain access to the Disabled tier'
    Assert-Matches $governor '(?s)_healthySamples >= HealthySamplesToRecover && cooldownComplete.*?NextHigherRenderTier\(_adaptiveTier\)' 'One-tier hysteretic recovery boundary'
    Assert-NotMatches $governor 'winrt::|Windows::UI|CoreDispatcher|SafeDispatcherTimer|DispatcherTimer|CompositionTarget::Rendering' 'Pure governor platform-isolation boundary'
    Assert-NotMatches $governor '(?i)TraceLogging|OutputDebugString|printf\s*\(|wprintf\s*\(|std::c(?:out|err)|Telemetry' 'Governor privacy and telemetry boundary'
    foreach ($threshold in @('UnhealthyDispatchLatency', 'HealthyDispatchLatency', 'RecoveryCooldown', 'UnhealthySamplesToDegrade', 'HealthySamplesToRecover'))
    {
        Assert-NotContains $source.PaneCpp $threshold 'Centralized governor threshold boundary'
    }

    $renderer = $source.Renderer
    foreach ($required in @(
        'class RainbowArcRenderer final',
        'TryCreate',
        '_initializeSolidFallback',
        '_initializeCompositionBase',
        '_initializeGradientStage',
        '_initializeHeadStage',
        '_initializeSparkStage',
        'SetHostWindowState',
        'CreateRoundedRectangleGeometry',
        'CreateInsetClip',
        '_rainbowFillVisual',
        '_rainbowBrush = _createRainbowBrush()',
        '_rainbowBrush.StartAnimation(L"Offset"',
        '_whiteCore',
        '_warmCore',
        '_headTrail',
        '_innerGlow',
        '_outerBloom',
        'std::array<SparkVisual, RainbowArcVisualConstants::SparkPoolCapacityPerPane>',
        '_sparkPool.Acquire',
        '_sparkPool.Release',
        'CreateScopedBatch',
        '_applyWithDegradation',
        '_restartGeometryAnimations',
        '_showErrorWithoutProgress',
        '_isLightTheme',
        'UsesStaticFallback',
        '_stopAllAnimations',
        '_releaseAllSparks',
        'SetPerformanceMode',
        'SetApplicationAnimationsEnabled',
        'SetRuntimeEnvironment',
        'ObserveDispatchLatency',
        '_performanceGovernor.ObserveDispatchLatency',
        '_performanceGovernor.ObserveHardFailure',
        'using FaultCallback = std::function<void()>',
        '_markFaulted()',
        '_degradeAfterFailure',
        '_showFallbackChecked',
        '_updateFallbackChecked',
        '_availableCapabilityTier',
        'MostRestrictiveRenderTier(_availableCapabilityTier, next)',
        'for (uint8_t attempt = 0; attempt < 5 && !_closed; ++attempt)',
        'void Close() noexcept',
        'inline static SparkBudget _sharedSparkBudget'
    ))
    {
        Assert-Contains $renderer $required 'Dedicated Rainbow Arc composition renderer'
    }
    Assert-Matches $renderer '(?s)auto callback = std::move\(_faultCallback\).*?_faultCallback = \{\};.*?callback\(\);' 'One-shot renderer fault notification'
    Assert-Matches $renderer '(?s)void _updateGeometry\(\) noexcept.*?catch \(\.\.\.\).*?_degradeAfterFailure' 'Geometry and simulated device-loss degradation boundary'
    Assert-Matches $renderer '(?s)RenderTier::Solid \|\| !_root.*?_showFallbackChecked\(true\).*?_updateFallbackChecked' 'Checked final fallback presentation boundary'
    if ([regex]::Matches($renderer, '_createRainbowBrush\(').Count -ne 2)
    {
        throw 'The Rainbow Arc gradient brush must have one initializer call and one cached-brush factory definition.'
    }
    foreach ($status in @('Running', 'Waiting', 'Success', 'Error'))
    {
        Assert-Matches $renderer "light\s*\?\s*RainbowArcVisualConstants::Light$($status)Solid\s*:\s*RainbowArcVisualConstants::$($status)Solid" "Light-surface $status fallback palette selection"
    }
    foreach ($forbidden in @(
        'CoreWindow::GetForCurrentThread',
        '_coreWindow.Visible'
    ))
    {
        Assert-NotContains $renderer $forbidden 'Authoritative XAML-Islands host-window lifecycle boundary'
    }
    Assert-NotMatches $renderer '_coreWindow\s*\.\s*(?:VisibilityChanged|Activated)\s*\(' 'Authoritative XAML-Islands host-window lifecycle boundary'

    foreach ($required in @(
        'using LaunchExpiredCallback = std::function<void(uint64_t)>',
        '_synchronizeLaunchClock',
        '_startLaunchClock',
        '_completeLaunchClock',
        '_clearLaunchClock',
        '_launchClockProperties.StartAnimation(L"Progress", _launchClockAnimation)',
        '_launchClockAnimation.Duration(_timeSpan(RainbowArcVisualConstants::IndeterminateCycleDuration))',
        '_cometTailAnimation.IterationCount(1)',
        '_cometHeadAnimation.IterationCount(1)',
        '_cometHeadOpacityAnimation.IterationCount(1)'
    ))
    {
        Assert-Contains $renderer $required 'Bounded one-shot shell launch clock'
    }
    Assert-Matches $renderer '(?s)const auto oneShotLaunch = _snapshot\.source == ProgressSource::ShellIntegration &&\s*_snapshot\.launchGeneration != 0;' 'One-shot launch comet source discrimination'
    Assert-NotContains $renderer '1800' 'Launch timeout shares the indeterminate traversal constant instead of a raw duration'

    $paneVisualStart = $source.PaneCpp.IndexOf('void Pane::_SetVisualProgressEnabled', [System.StringComparison]::Ordinal)
    $paneVisualEnd = $source.PaneCpp.IndexOf('void Pane::_UpdatePaneHeader', $paneVisualStart, [System.StringComparison]::Ordinal)
    if ($paneVisualStart -lt 0 -or $paneVisualEnd -le $paneVisualStart)
    {
        throw 'The Pane Visual Progress integration boundary could not be isolated.'
    }
    $paneVisualProgress = $source.PaneCpp.Substring($paneVisualStart, $paneVisualEnd - $paneVisualStart)
    $visualImplementation = "$renderer`n$renderModel`n$constants`n$paneVisualProgress"
    foreach ($forbidden in @('DispatcherTimer', 'CompositionTarget::Rendering', 'Storyboard'))
    {
        Assert-NotContains $visualImplementation $forbidden 'Visual Progress CPU frame-loop and per-pane timer boundary'
    }
    foreach ($forbidden in @('std::thread', 'std::jthread', 'std::async', 'CreateThread', 'ThreadPool'))
    {
        Assert-NotContains "$renderer`n$renderModel`n$($source.Recognition)" $forbidden 'Visual Progress worker boundary'
    }
    Assert-NotContains $visualImplementation 'GaussianBlur' 'Localized glow boundary'
    Assert-NotMatches "$renderer`n$renderModel" 'std::vector\s*<\s*(?:Spark|SparkVisual|Slot)|std::deque\s*<\s*(?:Spark|SparkVisual|Slot)' 'Fixed particle-container boundary'
    Assert-NotMatches $renderer '\bnew\s+(?:Spark|SparkVisual)|make_(?:unique|shared)\s*<\s*(?:Spark|SparkVisual)' 'Per-burst particle-allocation boundary'
    foreach ($codePoint in 0x2580..0x259f)
    {
        Assert-NotContains $visualImplementation ([string][char]$codePoint) 'Continuous geometry block-glyph boundary'
    }
    Assert-NotContains $visualImplementation ([string][char]0x2501) 'Continuous geometry heavy-line glyph boundary'

    $coordinator = $source.Coordinator
    $samplerState = $source.SamplerState
    $safeDispatcherTimer = $source.SafeDispatcherTimer
    foreach ($required in @(
        'class VisualProgressWindowCoordinator final',
        'SafeDispatcherTimer _timer',
        'SamplerInterval{ 1000 }',
        'void SetEligible(const bool eligible) noexcept',
        'if (!eligible)',
        '_stop();',
        '_timer.Destroy()',
        '_state.Start()',
        '_state.Stop()',
        '_state.TryBeginProbe(generation)',
        '_state.TryCompleteProbe(generation)',
        'CoreDispatcherPriority::Low',
        'weak_from_this()',
        'ActiveSamplerCount()',
        '~VisualProgressWindowCoordinator()',
        'Close();'
    ))
    {
        Assert-Contains $coordinator $required 'Single window-scoped low-frequency sampler'
    }
    Assert-Matches $coordinator '(?s)if \(!eligible\).*?_stop\(\);.*?return;' 'Immediate ineligible sampler shutdown'
    Assert-Before $coordinator '_activeSamplerCount.fetch_add(1' '_timer.Interval(' 'Sampler active-count increment precedes throwable timer setup'
    foreach ($required in @(
        '~SafeDispatcherTimer() noexcept',
        'void Destroy() noexcept',
        'std::exchange(_timer, nullptr)',
        'std::exchange(_token, {})',
        'timer.Stop()',
        'timer.Tick(token)',
        'catch (...)'
    ))
    {
        Assert-Contains $safeDispatcherTimer $required 'No-throw dispatcher timer teardown'
    }
    foreach ($required in @(
        'class VisualProgressSamplerState final',
        'bool Start() noexcept',
        'bool Stop() noexcept',
        'bool Close() noexcept',
        'bool TryBeginProbe(uint64_t& generation) noexcept',
        'bool TryCompleteProbe(const uint64_t generation) noexcept',
        'if (_closed || !_running || _probePending)',
        'generation != _generation',
        '_probePending = false',
        '++_generation'
    ))
    {
        Assert-Contains $samplerState $required 'Pure sampler pending and generation state'
    }
    Assert-Matches $samplerState '(?s)if \(_closed \|\| !_running \|\| _probePending\).*?return false;.*?_probePending = true;.*?generation = _generation' 'Single pending UI-dispatch probe boundary'
    Assert-Matches $samplerState '(?s)generation != _generation.*?return false;' 'Stale sampler-generation discard boundary'
    Assert-NotContains "$($source.PaneCpp)`n$($source.PaneH)`n$renderer" 'SafeDispatcherTimer' 'No permanent per-pane or renderer sampler'
    Assert-NotContains "$coordinator`n$($source.TerminalPageCpp)" 'CompositionTarget::Rendering' 'No CPU frame callback for governor sampling'
    Assert-NotMatches $coordinator 'std::thread|std::jthread|std::async|CreateThread|ThreadPool' 'Window sampler worker-thread boundary'
    Assert-Contains $source.TerminalPageH 'std::shared_ptr<winTerm::VisualProgress::VisualProgressWindowCoordinator> _visualProgressWindowCoordinator' 'One coordinator owned per TerminalPage window'
    Assert-Matches $source.TerminalPageCpp '(?s)const auto samplerEligible = _visible\s*&&\s*_activated\s*&&\s*\(activeProgressCount != 0 \|\| accessibilityTickNeeded\)' 'Visible active-progress or bounded accessibility-deadline sampler eligibility'
    Assert-Contains $source.TerminalPageCpp '_visualProgressWindowCoordinator->SetEligible(samplerEligible)' 'Window sampler live start-stop propagation'
    Assert-Contains $source.TerminalPageCpp 'pane->TickVisualProgressAccessibility()' 'Shared sampler accessibility deadline reevaluation'
    Assert-Matches $source.TerminalPageCpp '(?s)if \(!tabFocused\).*?pane->SetVisualProgressActivePaneCount\(0\)' 'Background-tab governor idle reset'
    Assert-NotContains $source.PaneH 'VisualProgressWindowCoordinator' 'No coordinator ownership per pane'
    Assert-Contains $source.TerminalAppProject '<ClInclude Include="VisualProgressWindowCoordinator.h" />' 'Window coordinator project inventory'
    Assert-Contains $source.TerminalAppFilters '<ClInclude Include="VisualProgressWindowCoordinator.h" />' 'Window coordinator IDE-filter inventory'

    $pane = $source.PaneCpp
    foreach ($required in @(
        'WINTERM_DISABLE_VISUAL_PROGRESS',
        'RainbowArcVisualConstants::OverlayHostHeight',
        '_visualProgressOverlay.IsHitTestVisible(false)',
        'Controls::Grid::SetRow(_visualProgressOverlay, 1)',
        'AccessibilityView::Raw',
        '_visualProgressSemanticProgress = Controls::ProgressBar{}',
        '_visualProgressSemanticProgress.Minimum(0.0)',
        '_visualProgressSemanticProgress.Maximum(100.0)',
        '_visualProgressSemanticProgress.IsTabStop(false)',
        'RS_(L"VisualProgress_AccessibleName")',
        'RainbowArcRenderer::TryCreate(',
        '[weakThis]()',
        'pane->_OnVisualProgressRendererFault()',
        'const auto rendererReady = _visualProgressRenderer && !_visualProgressRenderer->Faulted()',
        '_visualProgressRendererReady.store(rendererReady',
        '_visualProgressRenderer->SetPaneActive(_lastActive)',
        '_visualProgressRenderer->SetPerformanceMode(_visualProgressPerformanceMode)',
        '_visualProgressRenderer->SetApplicationAnimationsEnabled(_visualProgressApplicationAnimationsEnabled)',
        '_visualProgressRenderer->RefreshEnvironment()',
        '_visualProgressRenderer->Apply(snapshot)',
        '_visualProgressRenderer->Close()',
        '_visualProgressMailbox.TakeLatest()',
        'std::weak_ptr<Pane>',
        'CoreDispatcherPriority::Low',
        '_paneVisualProgressProviderChangedRevoker.revoke()',
        '_visualProgressState.Reset()',
        '_visualProgressMailbox.Close()',
        '_DestroyVisualProgressOverlay()'
    ))
    {
        Assert-Contains $pane $required 'Per-pane Rainbow Arc integration'
    }
    $configureStart = $pane.IndexOf('void Pane::_ConfigureVisualProgressRecognition', [System.StringComparison]::Ordinal)
    $configureEnd = $pane.IndexOf('void Pane::_QueueVisualProgressUpdate', $configureStart, [System.StringComparison]::Ordinal)
    if ($configureStart -lt 0 -or $configureEnd -le $configureStart)
    {
        throw 'The Pane Visual Progress recognition configuration boundary could not be isolated.'
    }
    $configureRecognition = $pane.Substring($configureStart, $configureEnd - $configureStart)
    $recognitionEnabledStart = $configureRecognition.IndexOf('const auto enabled', [System.StringComparison]::Ordinal)
    $recognitionEnabledEnd = $configureRecognition.IndexOf(';', $recognitionEnabledStart, [System.StringComparison]::Ordinal)
    if ($recognitionEnabledStart -lt 0 -or $recognitionEnabledEnd -le $recognitionEnabledStart)
    {
        throw 'The CLI recognition-enabled gate could not be isolated.'
    }
    $recognitionEnabledGate = $configureRecognition.Substring($recognitionEnabledStart, $recognitionEnabledEnd - $recognitionEnabledStart)
    Assert-Contains $recognitionEnabledGate '_visualProgressEnabled.load' 'Master feature recognition gate'
    Assert-Contains $recognitionEnabledGate '_visualProgressRecognizeCliProgress.load' 'Independent CLI-recognition setting gate'
    foreach ($forbidden in @('_visualProgressRendererReady', '_visualProgressFaulted', '_visualProgressReplaceRecognizedOutput'))
    {
        Assert-NotContains $recognitionEnabledGate $forbidden 'Recognition inspection independence from renderer and replacement'
    }
    Assert-Matches $configureRecognition '(?s)ConfigureVisualProgressRecognition\(\s*enabled,\s*enabled\s*&&\s*_visualProgressRendererReady\.load\([^;]+&&\s*!_visualProgressFaulted\.load\([^;]+&&\s*_visualProgressReplaceRecognizedOutput\.load' 'Renderer-ready replacement gate independent from recognition inspection'
    Assert-NotContains $pane '_visualProgressOverlay.Height(6.0)' 'Centralized overlay geometry boundary'
    Assert-NotContains $pane '_visualProgressOverlay.RowDefinitions' 'Viewport-preserving overlay boundary'
    if ([regex]::Matches($pane, 'RainbowArcRenderer::TryCreate\(').Count -ne 1)
    {
        throw 'Pane must have exactly one lifecycle-owned renderer creation site.'
    }

    $disableVisualStart = $pane.IndexOf('void Pane::_DisableVisualProgressOnUI', [System.StringComparison]::Ordinal)
    $disableVisualEnd = $pane.IndexOf('void Pane::_UpdatePaneHeader', $disableVisualStart, [System.StringComparison]::Ordinal)
    if ($disableVisualStart -lt 0 -or $disableVisualEnd -le $disableVisualStart)
    {
        throw 'The decorative renderer-failure boundary could not be isolated.'
    }
    $disableVisualProgress = $pane.Substring($disableVisualStart, $disableVisualEnd - $disableVisualStart)
    Assert-Contains $disableVisualProgress '_visualProgressFaulted.store(true' 'Terminal renderer-failure latch'
    Assert-Contains $disableVisualProgress '_visualProgressRenderer->Close()' 'Renderer failure resource release'
    Assert-Contains $disableVisualProgress '_visualProgressRenderer.reset()' 'Renderer failure ownership release'
    Assert-NotContains $disableVisualProgress 'RainbowArcRenderer::TryCreate' 'No renderer retry loop after a decorative fault'
    Assert-NotMatches $disableVisualProgress '_content\s*=|_content\.Close|GetTermControl\(\)\.Close|_connection' 'Decorative-only renderer fault isolation'
    foreach ($required in @(
        'void SetVisualProgressHostWindowState(bool visible, bool focused) noexcept',
        '_visualProgressHostWindowVisible',
        '_visualProgressHostWindowFocused'
    ))
    {
        Assert-Contains $source.PaneH $required 'Per-pane authoritative host-window state'
    }
    foreach ($required in @(
        'void Pane::SetVisualProgressHostWindowState',
        '_firstChild->SetVisualProgressHostWindowState',
        '_secondChild->SetVisualProgressHostWindowState',
        '_visualProgressRenderer->SetHostWindowState'
    ))
    {
        Assert-Contains $pane $required 'Pane-tree host-window lifecycle propagation'
    }

    $splitStart = $pane.IndexOf('Pane::_Split(', [System.StringComparison]::Ordinal)
    $splitEnd = $pane.IndexOf('Pane::_CreateMinSizeTree', $splitStart, [System.StringComparison]::Ordinal)
    if ($splitStart -lt 0 -or $splitEnd -le $splitStart)
    {
        throw 'The Pane split boundary could not be isolated.'
    }
    $splitImplementation = $pane.Substring($splitStart, $splitEnd - $splitStart)
    Assert-Contains $splitImplementation '_firstChild->SetVisualProgressHostWindowState' 'First split-child host-window state propagation'
    Assert-Contains $splitImplementation '_secondChild->SetVisualProgressHostWindowState' 'Second split-child host-window state propagation'
    Assert-Contains $splitImplementation '_CopyVisualProgressConfigurationTo(_firstChild)' 'Leaf and nested-wrapper Visual Progress configuration propagation'
    foreach ($required in @(
        'void Pane::_CopyVisualProgressConfigurationTo',
        'pane->_visualProgressRecognizeCliProgress.store',
        'pane->_visualProgressReplaceRecognizedOutput.store',
        'pane->_visualProgressPerformanceMode = _visualProgressPerformanceMode',
        'pane->_visualProgressApplicationAnimationsEnabled = _visualProgressApplicationAnimationsEnabled',
        'pane->_visualProgressSoftwareRendering = _visualProgressSoftwareRendering',
        'pane->_visualProgressActivePaneCount.store',
        'pane->_SetVisualProgressEnabled'
    ))
    {
        Assert-Contains $pane $required 'Relocated leaf and nested-wrapper Visual Progress state propagation'
    }
    Assert-Contains $source.TabManagement 'newTabImpl->UpdateSettings(_settings)' 'Current settings applied to newly created tabs'
    Assert-Matches $source.TerminalPageCpp '(?s)newPane->WalkTree\(\[&\]\(const auto& pane\).*?pane->UpdateSettings\(_settings\)' 'Current settings applied to every node of a newly split pane subtree'
    foreach ($required in @(
        'CompositionCapabilities::GetForCurrentView().AreEffectsFast()',
        'PowerManager::EnergySaverStatus()',
        '_visualProgressTabVisible',
        '_visualProgressElementLoaded',
        '_IsVisualProgressPresented()',
        '_leafLayout.Loaded(',
        '_leafLayout.Unloaded(',
        'SetVisualProgressTabVisible'
    ))
    {
        Assert-Contains $pane $required 'Platform and presented-pane governor adapter'
    }
    Assert-Matches $pane '(?s)_visualProgressFaulted\.store\(false.*?_AttachLeafVisual\(\);.*?_RehydrateVisualProgressState\(\);' 'Bounded split-collapse renderer recovery and survivor rehydration'
    Assert-Matches $pane '(?s)void Pane::_OnVisualProgressRendererFault.*?_visualProgressFaulted\.store\(true.*?_visualProgressRendererReady\.store\(false.*?_ConfigureVisualProgressRecognition\(\).*?RunAsync.*?pane->_DisableVisualProgressOnUI\(\)' 'Immediate replacement revocation with reentrancy-safe deferred renderer ownership release'
    Assert-NotContains $pane 'faultedRenderer->Close()' 'No synchronous renderer destruction from an in-flight fault callback'
    $faultCallbackStart = $pane.IndexOf('void Pane::_OnVisualProgressRendererFault()', [System.StringComparison]::Ordinal)
    $faultCallbackEnd = $pane.IndexOf('bool Pane::_HandleVisualProgressRendererFault()', $faultCallbackStart, [System.StringComparison]::Ordinal)
    if ($faultCallbackStart -lt 0 -or $faultCallbackEnd -le $faultCallbackStart)
    {
        throw 'The renderer fault-callback boundary could not be isolated.'
    }
    $faultCallback = $pane.Substring($faultCallbackStart, $faultCallbackEnd - $faultCallbackStart)
    Assert-NotContains $faultCallback 'VisualProgressActivityChanged.raise' 'No synchronous page reentry from an in-flight renderer fault callback'
    $rehydrateStart = $pane.IndexOf('void Pane::_RehydrateVisualProgressState', [System.StringComparison]::Ordinal)
    $rehydrateEnd = $pane.IndexOf('void Pane::_CreateVisualProgressOverlay', $rehydrateStart, [System.StringComparison]::Ordinal)
    if ($rehydrateStart -lt 0 -or $rehydrateEnd -le $rehydrateStart)
    {
        throw 'The split-collapse Visual Progress rehydration boundary could not be isolated.'
    }
    $rehydrate = $pane.Substring($rehydrateStart, $rehydrateEnd - $rehydrateStart)
    Assert-Before $rehydrate '_UpdateVisualProgressFromShellIntegration()' '_UpdateVisualProgressFromTaskbar()' 'Shell-before-Taskbar rehydration precedence'
    Assert-Before $rehydrate '_UpdateVisualProgressFromTaskbar()' '_UpdateVisualProgressFromProvider()' 'Taskbar-before-Provider source refresh order'
    $takeContentStart = $pane.IndexOf('IPaneContent Pane::_takePaneContent()', [System.StringComparison]::Ordinal)
    $takeContentEnd = $pane.IndexOf('void Pane::_setPaneContent', $takeContentStart, [System.StringComparison]::Ordinal)
    if ($takeContentStart -lt 0 -or $takeContentEnd -le $takeContentStart)
    {
        throw 'The pane-content Visual Progress transfer boundary could not be isolated.'
    }
    $takeContent = $pane.Substring($takeContentStart, $takeContentEnd - $takeContentStart)
    Assert-Matches $takeContent '(?s)const auto recognitionEnabled = _visualProgressEnabled\.load.*?_visualProgressRecognizeCliProgress\.load.*?ConfigureVisualProgressRecognition\(recognitionEnabled, false\)' 'Ownership transfer keeps bounded provider inspection while revoking suppression'
    Assert-NotContains $takeContent 'ConfigureVisualProgressRecognition(false, false)' 'Ownership transfer does not erase the current provider before rehydration'
    Assert-Matches $pane '(?s)_paneVisualProgressProviderChangedRevoker = terminalContent\.VisualProgressProviderChanged.*?if \(_visualProgressEnabled\.load.*?_ConfigureVisualProgressRecognition\(\);' 'Transferred recognizer state is preserved until actual pane settings are applied'

    $shellProgressStart = $pane.IndexOf('void Pane::_UpdateVisualProgressFromShellIntegration', [System.StringComparison]::Ordinal)
    $shellProgressEnd = $pane.IndexOf('void Pane::_UpdateVisualProgressFromProvider', $shellProgressStart, [System.StringComparison]::Ordinal)
    if ($shellProgressStart -lt 0 -or $shellProgressEnd -le $shellProgressStart)
    {
        throw 'The explicit shell/OSC progress boundary could not be isolated.'
    }
    $shellProgress = $pane.Substring($shellProgressStart, $shellProgressEnd - $shellProgressStart)
    Assert-Contains $shellProgress 'ApplyShellLifecycle' 'OSC 133 shell lifecycle remains active'
    Assert-NotContains $shellProgress '_visualProgressRecognizeCliProgress' 'OSC 133 independence from CLI recognition setting'

    $taskbarProgressStart = $pane.IndexOf('void Pane::_UpdateVisualProgressFromTaskbar', [System.StringComparison]::Ordinal)
    $taskbarProgressEnd = $pane.IndexOf('void Pane::_UpdateVisualProgressFromShellIntegration', $taskbarProgressStart, [System.StringComparison]::Ordinal)
    if ($taskbarProgressStart -lt 0 -or $taskbarProgressEnd -le $taskbarProgressStart)
    {
        throw 'The explicit OSC 9;4 progress boundary could not be isolated.'
    }
    $taskbarProgress = $pane.Substring($taskbarProgressStart, $taskbarProgressEnd - $taskbarProgressStart)
    Assert-Contains $taskbarProgress 'ApplyTaskbar' 'OSC 9;4 taskbar-state progress remains active'
    Assert-NotContains $taskbarProgress '_visualProgressRecognizeCliProgress' 'OSC 9;4 independence from CLI recognition setting'

    Assert-Matches $pane '(?s)RainbowArcRenderer::TryCreate\(.*?_OnVisualProgressRendererFault\(\);.*?\[weakThis\]\(const uint64_t launchGeneration\).*?_ExpireVisualProgressShellLaunch\(launchGeneration\);' 'Weak one-shot launch expiration callback registration'
    Assert-Matches $pane '(?s)void Pane::_ExpireVisualProgressShellLaunch.*?ExpireShellLaunch\(launchGeneration\).*?_QueueVisualProgressUpdate' 'Launch expiration routed through the state machine and UI mailbox'

    $terminalPage = $source.TerminalPageCpp
    $registerStart = $terminalPage.IndexOf('void TerminalPage::_RegisterTabEvents', [System.StringComparison]::Ordinal)
    $registerEnd = $terminalPage.IndexOf('void TerminalPage::_UnZoomIfNeeded', $registerStart, [System.StringComparison]::Ordinal)
    $visibilityStart = $terminalPage.IndexOf('void TerminalPage::WindowVisibilityChanged', [System.StringComparison]::Ordinal)
    $visibilityEnd = $terminalPage.IndexOf('void TerminalPage::_Find', $visibilityStart, [System.StringComparison]::Ordinal)
    $activationStart = $terminalPage.IndexOf('void TerminalPage::WindowActivated', [System.StringComparison]::Ordinal)
    $activationEnd = $terminalPage.IndexOf('safe_void_coroutine TerminalPage::_ControlCompletionsChangedHandler', $activationStart, [System.StringComparison]::Ordinal)
    if ($registerStart -lt 0 -or $registerEnd -le $registerStart -or
        $visibilityStart -lt 0 -or $visibilityEnd -le $visibilityStart -or
        $activationStart -lt 0 -or $activationEnd -le $activationStart)
    {
        throw 'The TerminalPage host-window lifecycle boundaries could not be isolated.'
    }
    $registerImplementation = $terminalPage.Substring($registerStart, $registerEnd - $registerStart)
    $visibilityImplementation = $terminalPage.Substring($visibilityStart, $visibilityEnd - $visibilityStart)
    $activationImplementation = $terminalPage.Substring($activationStart, $activationEnd - $activationStart)
    Assert-Contains $registerImplementation 'GetRootPane()->SetVisualProgressHostWindowState(_visible, _activated)' 'Initial tab host-window state propagation'
    Assert-Contains $visibilityImplementation '_visible = showOrHide' 'Authoritative window visibility state'
    Assert-Contains $visibilityImplementation 'GetRootPane()->SetVisualProgressHostWindowState(_visible, _activated)' 'Window visibility propagation'
    Assert-Contains $activationImplementation '_activated = activated' 'Authoritative window activation state'
    Assert-Contains $activationImplementation 'GetRootPane()->SetVisualProgressHostWindowState(_visible, _activated)' 'Window activation propagation'

    $recognition = $source.Recognition
    foreach ($required in @(
        'struct RecognitionOptions',
        'replacementEnabled',
        'rendererEnabled',
        'normalScreen',
        'parserHealthy',
        'struct RecognitionResult',
        'suppressInput',
        'overflow',
        'healthy',
        'accepted',
        'class RecognitionEngine final',
        'RecognitionResult Consume',
        'std::try_to_lock',
        'if (!lock.owns_lock())',
        'MaxChunkCodeUnits = 32 * 1024',
        'MaxCurrentLineCodeUnits = 2048',
        'MaxAnsiSequenceCodeUnits = 64',
        'RecentProgressCapacity = 8',
        'DockerLayerCapacity = 32',
        'BuildKitStepCapacity = 32',
        'PublicationIntervalMilliseconds = 50',
        'std::array<wchar_t, MaxCurrentLineCodeUnits>',
        'std::array<wchar_t, MaxAnsiSequenceCodeUnits>',
        'std::array<LayerState, DockerLayerCapacity>',
        'std::array<StepState, BuildKitStepCapacity>',
        'progress.confidence == ProviderConfidence::High',
        'progress.provider == ProgressProvider::Pip',
        'progress.provider == ProgressProvider::Git',
        'progress.provider == ProgressProvider::Curl',
        'progress.provider == ProgressProvider::Wget',
        'progress.provider != ProgressProvider::Generic',
        'class Utf8RecognitionAdapter final',
        'MaxChunkBytes',
        'bool TryReset() noexcept',
        '_findSharedUnitQuantityFraction',
        '_hasTransferRateAndEta',
        'pipSizedDownload',
        'resolverTransfer',
        'resolverProgress',
        '_isSpinner',
        '_genericTransientShape',
        '_resetGenericHeuristics',
        '_hasActiveHighConfidenceBuiltInClaim',
        '_providerClear',
        'pendingCandidate'
    ))
    {
        Assert-Contains $recognition $required 'Bounded fail-open recognition framework'
    }
    Assert-Matches $recognition '(?s)result\.suppressInput = immediateWholeChunk\s*&&\s*options\.replacementEnabled\s*&&\s*options\.rendererEnabled\s*&&\s*options\.normalScreen\s*&&\s*options\.parserHealthy\s*&&\s*result\.healthy' 'Immediate safe-suppression gates'
    Assert-NotContains $recognition '#include <regex>' 'Bounded recognizer regular-expression boundary'
    Assert-NotContains $recognition 'std::regex' 'Bounded recognizer regular-expression boundary'
    Assert-NotMatches $recognition '(?i)TraceLogging|OutputDebugString|printf\s*\(|wprintf\s*\(|std::c(?:out|err)|LOG_[A-Z_]*\s*\(' 'Recognition privacy and output-logging boundary'

    $controlCore = $source.ControlCoreCpp
    foreach ($required in @(
        '../../winterm/VisualProgress/ProgressRecognition.h',
        'ConfigureVisualProgressRecognition',
        'std::make_unique<winTerm::VisualProgress::RecognitionEngine>()',
        'std::unique_lock recognitionLock{ _visualProgressRecognitionMutex, std::try_to_lock }',
        '_visualProgressRecognition->Consume(output',
        '_terminal->Write(output)',
        '_terminal->IsInAlternateScreenBuffer()',
        '!wasAlternateScreen',
        'wasAlternateScreen || isAlternateScreen',
        '_visualProgressRecognitionGeneration',
        '_visualProgressProviderGeneration',
        '== recognitionGeneration',
        '_visualProgressRecognitionResetRequested',
        '_visualProgressSuppressionMutex',
        '_visualProgressReplacementEnabled.store(false',
        '_visualProgressRecognitionEnabled.store(false',
        '_visualProgressRecognition.reset()',
        '_visualProgressProviderState.exchange(0',
        'const auto resetSinceInspection',
        'const auto unsafeIngress',
        'const auto discardRecognition',
        'recognition.accepted',
        'recognition.healthy',
        'recognition.overflow',
        'PackProviderProgress',
        'VisualProgressProviderChanged.raise'
    ))
    {
        Assert-Contains $controlCore $required 'Terminal-output recognition fail-open integration'
    }
    Assert-Before $controlCore '_visualProgressRecognition->Consume(output' '_terminal->Write(output)' 'Immediate recognition decision boundary'
    $handlerStart = $controlCore.IndexOf('void ControlCore::_connectionOutputHandler', [System.StringComparison]::Ordinal)
    $handlerEnd = $controlCore.IndexOf('::Microsoft::Console::Render::Renderer* ControlCore::GetRenderer', $handlerStart, [System.StringComparison]::Ordinal)
    if ($handlerStart -lt 0 -or $handlerEnd -le $handlerStart)
    {
        throw 'The terminal-output recognition handler boundary could not be isolated.'
    }
    $outputHandler = $controlCore.Substring($handlerStart, $handlerEnd - $handlerStart)
    Assert-NotMatches $outputHandler '(?i)TraceLogging|OutputDebugString|printf\s*\(|wprintf\s*\(|std::c(?:out|err)|Log(?:Terminal|Output|Command)' 'Terminal-output privacy boundary'
    Assert-Matches $controlCore '(?s)if \(!enabled\).*?_visualProgressReplacementEnabled\.store\(false.*?_visualProgressRecognitionEnabled\.store\(false.*?_visualProgressRecognitionGeneration\.fetch_add\(1.*?_visualProgressRecognitionResetRequested\.store\(true.*?_visualProgressProviderState\.exchange\(0' 'Recognition-disable suppression, generation, reset, and provider-clear boundary'
    Assert-Before $controlCore 'if (!enabled)' 'std::make_unique<winTerm::VisualProgress::RecognitionEngine>()' 'Recognizer is not initialized while CLI recognition is disabled'
    Assert-Matches $outputHandler '(?s)maySuppress = suppressionGate\.owns_lock\(\).*?inspected.*?recognition\.accepted.*?recognition\.healthy.*?recognition\.suppressInput.*?_visualProgressRecognitionEnabled\.load.*?_visualProgressReplacementEnabled\.load.*?!_visualProgressRecognitionResetRequested\.load.*?_visualProgressRecognitionGeneration\.load\([^;]+== recognitionGeneration.*?!wasAlternateScreen' 'Final synchronous replacement generation and safety gate'
    Assert-Matches $outputHandler '(?s)if \(!maySuppress\).*?_terminal->Write\(output\)' 'Mandatory raw-output fail-open path'
    Assert-Matches $outputHandler '(?s)_visualProgressProviderGeneration\.store\(recognitionGeneration.*?_visualProgressProviderState\.exchange\(packed.*?_visualProgressRecognitionGeneration\.load\([^;]+!= recognitionGeneration.*?_visualProgressProviderState\.compare_exchange_strong' 'Provider publication stale-generation rollback'

    Assert-Contains $source.TerminalHpp 'bool IsInAlternateScreenBuffer() const noexcept' 'Read-only alternate-screen API'
    Assert-Contains $source.TerminalCpp 'bool Microsoft::Terminal::Core::Terminal::IsInAlternateScreenBuffer() const noexcept' 'Read-only alternate-screen implementation'
    Assert-Contains $source.TerminalCpp 'return _inAltBuffer();' 'Read-only alternate-screen state'

    foreach ($required in @(
        'UInt64 VisualProgressProviderState { get; };'
    ))
    {
        Assert-Contains $source.ICoreStateIdl $required 'Structural provider-state ABI'
        Assert-Contains $source.TerminalPaneContentIdl $required 'Pane provider-state ABI'
    }
    foreach ($idl in @($source.ControlCoreIdl, $source.TermControlIdl))
    {
        Assert-Contains $idl 'ConfigureVisualProgressRecognition(Boolean enabled, Boolean replaceRecognizedOutput)' 'Recognition configuration ABI'
        Assert-Contains $idl 'VisualProgressProviderChanged' 'Provider-state event ABI'
    }
    Assert-Contains $source.TerminalPaneContentIdl 'VisualProgressProviderChanged' 'Pane provider-state event ABI'
    Assert-Contains $source.TermControlCpp '_core.ConfigureVisualProgressRecognition(enabled, replaceRecognizedOutput)' 'TermControl recognition forwarding'
    Assert-Contains $source.TermControlCpp 'return _core.VisualProgressProviderState()' 'TermControl provider-state forwarding'
    Assert-Contains $source.TerminalPaneContentCpp 'VisualProgressProviderChanged.raise' 'Pane provider-state event bubbling'

    $accessibility = $source.Accessibility
    foreach ($required in @(
        'enum class AccessibilityAnnouncement',
        'Started,',
        'Progress25,',
        'Progress50,',
        'Progress75,',
        'Progress100,',
        'Waiting,',
        'Success,',
        'Error,',
        'Cancelled,',
        'struct AccessibilitySemantics',
        'bool hasNumericValue{}',
        'bool IsIndeterminate() const noexcept',
        'class VisualProgressAccessibilityPolicy final',
        'MinimumAnnouncementInterval{ 4000 }',
        '_consumeMilestone(snapshot.value, 25',
        '_consumeMilestone(snapshot.value, 50',
        '_consumeMilestone(snapshot.value, 75',
        '_consumeMilestone(snapshot.value, 100',
        'const auto paneEligible = paneVisible && paneActive',
        'if (!paneEligible)',
        '_terminalObserved',
        '_pendingAnnouncement',
        'HasPendingAnnouncement() const noexcept',
        'void Reset() noexcept'
    ))
    {
        Assert-Contains $accessibility $required 'Bounded structural accessibility policy'
    }
    Assert-Matches $accessibility '(?s)visible && mode == ProgressMode::Determinate' 'Determinate-only numeric automation value'
    Assert-Matches $accessibility '(?s)return visible && mode == ProgressMode::Indeterminate;' 'Indeterminate automation state without fabricated percentage'
    Assert-Matches $accessibility '(?s)if \(!_terminalObserved\).*?_terminalObserved = true;.*?update\.announcement = terminal;' 'Single terminal announcement boundary'
    Assert-Matches $accessibility '(?s)snapshot\.status != ProgressStatus::Waiting.*?_pendingAnnouncement == AccessibilityAnnouncement::Waiting.*?_deferredAnnouncement == AccessibilityAnnouncement::Waiting' 'Stale deferred Waiting cancellation when work resumes'
    Assert-NotMatches $accessibility 'std::wstring|std::string|winrt::hstring|Windows::UI|AutomationPeer' 'Accessibility policy text-retention and platform-isolation boundary'

    foreach ($required in @(
        'x:Key="VisualProgressSemanticProgressBarStyle"',
        'TargetType="ProgressBar"',
        '<Setter Property="IsHitTestVisible" Value="False" />',
        '<Setter Property="IsTabStop" Value="False" />'
    ))
    {
        Assert-Contains $source.AppXaml $required 'Non-focusable semantic ProgressBar style'
    }
    foreach ($required in @(
        '_visualProgressSemanticProgress = Controls::ProgressBar{}',
        'AccessibilityView::Content',
        '_visualProgressSemanticProgress.IsIndeterminate(update.semantics.IsIndeterminate())',
        'if (update.semantics.hasNumericValue)',
        '_visualProgressSemanticProgress.Value(update.semantics.value)',
        'AutomationProperties::SetItemStatus',
        'FrameworkElementAutomationPeer::CreatePeerForElement',
        'peer.RaiseNotificationEvent',
        'AutomationNotificationProcessing::MostRecent',
        'NeedsVisualProgressAccessibilityTick()',
        'TickVisualProgressAccessibility()',
        'const auto paneAnnouncer = paneVisible',
        '_visualProgressHostWindowFocused',
        '_lastActive'
    ))
    {
        Assert-Contains $pane $required 'Semantic UI Automation progress integration'
    }
    Assert-Matches $pane '(?s)if \(update\.semantics\.hasNumericValue\).*?_visualProgressSemanticProgress\.Value\(update\.semantics\.value\)' 'No fabricated numeric UIA value for indeterminate progress'
    Assert-Matches $pane '(?s)const auto paneAnnouncer = paneVisible\s*&&\s*_visualProgressHostWindowFocused\s*&&\s*_lastActive' 'Active-visible-pane-only live announcements'
    Assert-Matches $pane '(?s)void Pane::_ApplyVisualProgressSnapshot.*?_ApplyVisualProgressAccessibility\(snapshot\);\s*_SetVisualProgressActivity\(activityActive\);' 'Accessibility pending-state update precedes synchronous sampler lifecycle refresh'
    Assert-Matches $pane '(?s)bool Pane::NeedsVisualProgressAccessibilityTick.*?_visualProgressActivityActive\.load.*?\|\|\s*_visualProgressAccessibility\.HasPendingAnnouncement\(\)' 'Shared accessibility clock remains eligible for semantic progress after renderer failure'
    Assert-NotMatches $pane '(?i)VisualProgressAnnouncementText\([^)]*(?:command|output|path|url|package|provider)' 'No terminal or provider content passed into Visual Progress announcements'

    [xml]$appResources = $source.AppResources
    $appResourceNames = @($appResources.root.data | ForEach-Object { [string]$_.name })
    foreach ($resourceName in @(
        'VisualProgress_AccessibleName',
        'VisualProgress_StatusRunning',
        'VisualProgress_StatusWaiting',
        'VisualProgress_StatusSuccess',
        'VisualProgress_StatusError',
        'VisualProgress_StatusCancelled',
        'VisualProgress_AnnouncementStarted',
        'VisualProgress_Announcement25',
        'VisualProgress_Announcement50',
        'VisualProgress_Announcement75',
        'VisualProgress_Announcement100',
        'VisualProgress_AnnouncementWaiting',
        'VisualProgress_AnnouncementSuccess',
        'VisualProgress_AnnouncementError',
        'VisualProgress_AnnouncementCancelled'
    ))
    {
        if ($appResourceNames -notcontains $resourceName)
        {
            throw "Localized Visual Progress automation resource '$resourceName' is missing."
        }
    }
    Assert-Contains $source.AppResources '<value>Command progress</value>' 'Localized generic accessible name without command text'

    foreach ($required in @(
        'VisualProgressEnabled, "visualProgress.enabled", true',
        'VisualProgressRecognizeCliProgress, "visualProgress.recognizeCliProgress", true',
        'VisualProgressPerformanceMode, "visualProgress.performanceMode", Model::VisualProgressPerformanceMode::Automatic',
        'VisualProgressReplaceRecognizedOutput, "visualProgress.replaceRecognizedOutput", false'
    ))
    {
        Assert-Contains $source.Settings $required 'Stable Visual Progress setting and default'
    }
    foreach ($required in @(
        'enum VisualProgressPerformanceMode',
        'Automatic,',
        'Full,',
        'Balanced,',
        'Minimal,',
        'INHERITABLE_SETTING(Boolean, VisualProgressEnabled)',
        'INHERITABLE_SETTING(Boolean, VisualProgressRecognizeCliProgress)',
        'INHERITABLE_SETTING(VisualProgressPerformanceMode, VisualProgressPerformanceMode)',
        'INHERITABLE_SETTING(Boolean, VisualProgressReplaceRecognizedOutput)'
    ))
    {
        Assert-Contains $source.SettingsIdl $required 'Stable Visual Progress setting projection'
    }
    foreach ($required in @(
        'JSON_ENUM_MAPPER(::winrt::Microsoft::Terminal::Settings::Model::VisualProgressPerformanceMode)',
        'pair_type{ "automatic", ValueType::Automatic }',
        'pair_type{ "full", ValueType::Full }',
        'pair_type{ "balanced", ValueType::Balanced }',
        'pair_type{ "minimal", ValueType::Minimal }'
    ))
    {
        Assert-Contains $source.SettingsSerialization $required 'Visual Progress performance-mode JSON mapping'
    }
    Assert-Contains $source.EnumMappingsIdl 'VisualProgressPerformanceMode> VisualProgressPerformanceMode { get; }' 'Settings UI enum projection ABI'
    Assert-Contains $source.EnumMappingsH 'VisualProgressPerformanceMode> VisualProgressPerformanceMode()' 'Settings UI enum projection declaration'
    Assert-Contains $source.EnumMappingsCpp 'DEFINE_ENUM_MAP(Model::VisualProgressPerformanceMode, VisualProgressPerformanceMode)' 'Settings UI enum projection implementation'

    $schema = $source.SettingsSchema | ConvertFrom-Json
    $globalSchemaProperties = $schema.'$defs'.Globals.properties
    $expectedSchemaDefaults = [ordered]@{
        'visualProgress.enabled' = $true
        'visualProgress.recognizeCliProgress' = $true
        'visualProgress.performanceMode' = 'automatic'
        'visualProgress.replaceRecognizedOutput' = $false
    }
    foreach ($entry in $expectedSchemaDefaults.GetEnumerator())
    {
        if ($globalSchemaProperties.PSObject.Properties.Name -notcontains $entry.Key)
        {
            throw "Settings schema is missing '$($entry.Key)'."
        }
        if ($globalSchemaProperties.($entry.Key).default -ne $entry.Value)
        {
            throw "Settings schema default for '$($entry.Key)' is not '$($entry.Value)'."
        }
    }
    $schemaModes = @($globalSchemaProperties.'visualProgress.performanceMode'.enum) -join ','
    if ($schemaModes -ne 'automatic,full,balanced,minimal')
    {
        throw "Settings schema Visual Progress modes are '$schemaModes'."
    }

    [xml]$settingsXaml = $source.GlobalAppearanceXaml
    $settingsNamespace = [System.Xml.XmlNamespaceManager]::new($settingsXaml.NameTable)
    $settingsNamespace.AddNamespace('local', 'using:Microsoft.Terminal.Settings.Editor')
    $xamlNamespace = 'http://schemas.microsoft.com/winfx/2006/xaml'
    $containers = @($settingsXaml.SelectNodes('//local:SettingContainer', $settingsNamespace))
    $expectedContainers = [ordered]@{
        VisualProgressEnabled = 'Globals_VisualProgressEnabled'
        VisualProgressRecognizeCliProgress = 'Globals_VisualProgressRecognizeCliProgress'
        VisualProgressPerformanceMode = 'Globals_VisualProgressPerformanceMode'
        VisualProgressReplaceRecognizedOutput = 'Globals_VisualProgressReplaceRecognizedOutput'
    }
    foreach ($entry in $expectedContainers.GetEnumerator())
    {
        $container = @($containers | Where-Object { $_.GetAttribute('Name', $xamlNamespace) -eq $entry.Key })
        if ($container.Count -ne 1)
        {
            throw "Settings UI must contain exactly one named '$($entry.Key)' SettingContainer."
        }
        if ($container[0].GetAttribute('Uid', $xamlNamespace) -ne $entry.Value)
        {
            throw "Settings UI '$($entry.Key)' does not use x:Uid '$($entry.Value)'."
        }
    }
    foreach ($required in @(
        'x:Uid="Globals_VisualProgressHeader"',
        'ItemsSource="{x:Bind ViewModel.VisualProgressPerformanceModeList, Mode=OneWay}"',
        'SelectedItem="{x:Bind ViewModel.CurrentVisualProgressPerformanceMode, Mode=TwoWay}"',
        'IsEnabled="{x:Bind ViewModel.VisualProgressEnabled, Mode=OneWay}"',
        'IsEnabled="{x:Bind ViewModel.VisualProgressReplacementAvailable, Mode=OneWay}"',
        'IsOn="{x:Bind ViewModel.VisualProgressReplaceRecognizedOutput, Mode=TwoWay}"'
    ))
    {
        Assert-Contains $source.GlobalAppearanceXaml $required 'Localized Visual Progress Settings UI binding'
    }
    foreach ($required in @(
        'VisualProgressReplacementAvailable { get; }',
        'PERMANENT_OBSERVABLE_PROJECTED_SETTING(Boolean, VisualProgressEnabled)',
        'PERMANENT_OBSERVABLE_PROJECTED_SETTING(Boolean, VisualProgressRecognizeCliProgress)',
        'PERMANENT_OBSERVABLE_PROJECTED_SETTING(Boolean, VisualProgressReplaceRecognizedOutput)'
    ))
    {
        Assert-Contains $source.GlobalAppearanceViewModelIdl $required 'Visual Progress Settings UI projection'
    }
    Assert-Contains $source.GlobalAppearanceViewModelH 'GETSET_BINDABLE_ENUM_SETTING(VisualProgressPerformanceMode' 'Visual Progress performance-mode binding'
    Assert-Contains $source.GlobalAppearanceViewModelCpp 'INITIALIZE_BINDABLE_ENUM_SETTING(VisualProgressPerformanceMode' 'Visual Progress localized performance-mode list'
    Assert-Contains $source.GlobalAppearanceViewModelCpp 'viewModelProperty == L"VisualProgressEnabled" || viewModelProperty == L"VisualProgressRecognizeCliProgress"' 'Settings UI dependency notification sources'
    Assert-Matches $source.GlobalAppearanceViewModelCpp '(?s)bool GlobalAppearanceViewModel::VisualProgressReplacementAvailable\(\).*?return VisualProgressEnabled\(\) && VisualProgressRecognizeCliProgress\(\);' 'Output replacement UI dependency without preference mutation'

    [xml]$settingsResources = $source.SettingsResources
    $settingsResourceValues = @{}
    foreach ($resource in $settingsResources.root.data)
    {
        $resourceName = [string]$resource.name
        if ($settingsResourceValues.ContainsKey($resourceName))
        {
            throw "Duplicate Settings resource '$resourceName'."
        }
        $settingsResourceValues[$resourceName] = [string]$resource.value
    }
    $requiredSettingsResources = @(
        'Globals_VisualProgressHeader.Text',
        'Globals_VisualProgressEnabled.Header',
        'Globals_VisualProgressEnabled.HelpText',
        'Globals_VisualProgressRecognizeCliProgress.Header',
        'Globals_VisualProgressRecognizeCliProgress.HelpText',
        'Globals_VisualProgressPerformanceMode.Header',
        'Globals_VisualProgressPerformanceMode.HelpText',
        'Globals_VisualProgressPerformanceModeAutomatic.Content',
        'Globals_VisualProgressPerformanceModeFull.Content',
        'Globals_VisualProgressPerformanceModeBalanced.Content',
        'Globals_VisualProgressPerformanceModeMinimal.Content',
        'Globals_VisualProgressReplaceRecognizedOutput.Header',
        'Globals_VisualProgressReplaceRecognizedOutput.HelpText'
    )
    foreach ($resourceName in $requiredSettingsResources)
    {
        if (-not $settingsResourceValues.ContainsKey($resourceName) -or [string]::IsNullOrWhiteSpace($settingsResourceValues[$resourceName]))
        {
            throw "Localized Settings resource '$resourceName' is missing or empty."
        }
    }
    Assert-Matches $settingsResourceValues['Globals_VisualProgressEnabled.Header'] '(?i)visual progress' 'Searchable localized Visual Progress label'
    Assert-Matches "$($settingsResourceValues['Globals_VisualProgressRecognizeCliProgress.Header']) $($settingsResourceValues['Globals_VisualProgressRecognizeCliProgress.HelpText'])" '(?i)command-line.*progress|progress.*command-line' 'Localized CLI progress-recognition explanation'
    Assert-Matches $settingsResourceValues['Globals_VisualProgressPerformanceMode.HelpText'] '(?i)automatic.*full.*balanced.*minimal' 'Localized visual-effects mode behavior'
    Assert-Matches $settingsResourceValues['Globals_VisualProgressReplaceRecognizedOutput.HelpText'] '(?i)(changes|changing).*visible.*output.*logs.*warnings.*errors.*prompts.*summaries' 'Localized visible-output replacement warning'
    Assert-Contains $source.SettingsIndexGenerator '"Microsoft::Terminal::Settings::Editor::GlobalAppearance"' 'Global Appearance settings-search page map'
    Assert-Contains $source.SettingsIndexGenerator 'ResourceName      = "$($settingContainer.Uid)/Header"' 'Settings search indexes SettingContainer localized headers'
    Assert-Contains $source.SettingsIndexGenerator 'ElementName       = $name' 'Settings search focuses named SettingContainers'

    foreach ($required in @(
        'ShellIntegrationMarkKind::Prompt',
        'ShellIntegrationMarkKind::CommandStart',
        'ShellIntegrationMarkKind::CommandExecuted',
        'ShellIntegrationMarkKind::CommandFinished'
    ))
    {
        Assert-Contains $source.Dispatch $required 'Semantic shell lifecycle boundary'
    }

    $tests = $source.Tests
    foreach ($required in @(
        'MapEveryTaskbarState',
        'ClampDeterminateValues',
        'SuppressDuplicateState',
        'ProviderProgressPrecedesShellLifecycle',
        'StandardProgressPrecedesProviderAndFallsBack',
        'ShellLaunchFallbackIsOneShotPerCommand',
        'ShellLaunchExpirationIgnoresStaleGenerations',
        'ExpiredShellLaunchDoesNotResurrectAfterOwnershipClears',
        'ShellLaunchInvalidationOnResetDisableAndClose',
        'ProviderStatePackingContainsOnlyStructuralFields',
        'RecognitionClassifiesProvidersAndGenericFallback',
        'RecognitionHandlesFragmentationAndMalformedInput',
        'RecognitionSuppressesOnlySafeWholeChunks',
        'RecognitionBoundsStateAndCoalescesUpdates',
        'RecognitionAppliesSuppressionSafetyMatrix',
        'RecognitionPreservesHighConfidenceOwnershipAndClearsGeneric',
        'RecognitionBootstrapsRichPipAndMavenResolver',
        'RecognitionHandlesGenericIndeterminateShapes',
        'RecognitionHandlesArbitraryProviderSplitsAndReset',
        'RendererPlansRealValuesRegressionAndIndeterminateMode',
        'RendererPlansStatusAndAccessibilityFallbacks',
        'RendererFailureAndCloseRemainPaneLocal',
        'SparkPoolsEnforcePaneAndGlobalCaps',
        'BackgroundAndHiddenPanesDoNotRequestSparkWork',
        'EmergencyOverridePrecedesSetting',
        'DisabledFeatureIgnoresEvents',
        'MultiplePanesRemainIndependent',
        'CloseAndDetachCleanupStopsUpdates',
        'SplitOrDetachResetClearsReusableState',
        'FeatureReloadDisablesAndReenablesCleanly',
        'MailboxCoalescesRapidUpdatesAndReleasesOnClose',
        'SettingsUseStableDefaultsAndRoundTrip',
        'GovernorAppliesModesAndEnvironmentCaps',
        'GovernorHysteresisIsBoundedAndDeterministic',
        'AccessibilitySemanticsAndAnnouncementsAreBounded',
        'Phase3StressCoverageRemainsBounded',
        'uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 50 }, uint8_t{ 99 }, uint8_t{ 100 }',
        'Utf8RecognitionAdapter',
        'MaxChunkCodeUnits + 1',
        'MaxCurrentLineCodeUnits + 1',
        'MaxAnsiSequenceCodeUnits + 1',
        'rendererEnabled = false',
        'normalScreen = false',
        'parserHealthy = false',
        'std::array<Fixture, 13>',
        'split < fixture.stream.size()',
        'i < 10000',
        'uint8_t{ 8 }',
        'uint8_t{ 24 }',
        'visualProgress.enabled',
        'visualProgress.recognizeCliProgress',
        'visualProgress.performanceMode',
        'visualProgress.replaceRecognizedOutput',
        'VisualProgressPerformanceMode::Automatic',
        'VisualProgressPerformanceMode::Full',
        'VisualProgressPerformanceMode::Balanced',
        'VisualProgressPerformanceMode::Minimal',
        'inputs.highContrast = true',
        'inputs.applicationAnimationsEnabled = false',
        'inputs.windowVisible = false',
        'inputs.windowMinimized = true',
        'inputs.paneActive = false',
        'inputs.windowFocused = false',
        'std::array<std::thread, 4>',
        'update < 25000',
        'splitCloseRehydrationHealthy',
        'VisualProgressSamplerState sampler',
        'rendererRecoveryHealthy',
        'governorRecoveryHealthy',
        'samplerLifecycleHealthy',
        'ObserveHardFailure',
        'VisualProgressPerformanceGovernor latencyFloor',
        'sample <= 12',
        'latencyFloor.AdaptiveTier()',
        'decision.shouldSample',
        'HasPendingAnnouncement()',
        'update < 100000',
        'record < 100000',
        'lifecycle < 10000'
    ))
    {
        Assert-Contains $tests $required 'Visual Progress Phase 3 compiled test coverage'
    }
    Assert-Matches $tests '(?s)explicitValues\["visualProgress\.enabled"\] = false.*?VERIFY_IS_FALSE\(explicitSettings->VisualProgressEnabled\(\)\)' 'Explicit Visual Progress disable round-trip coverage'
    Assert-Matches $tests '(?s)explicitValues\["visualProgress\.recognizeCliProgress"\] = false.*?VERIFY_IS_FALSE\(explicitSettings->VisualProgressRecognizeCliProgress\(\)\)' 'Explicit CLI-recognition disable round-trip coverage'
    Assert-Matches $tests '(?s)invalid\["visualProgress\.performanceMode"\] = "maximum".*?VERIFY_THROWS' 'Invalid performance-mode repository-convention coverage'
    Assert-NotMatches $tests 'std::this_thread::sleep|Sleep\s*\(|Task\.Delay' 'Deterministic Visual Progress unit-test clock boundary'
    Assert-Contains $source.TestProject '<ClCompile Include="WinTermVisualProgressTests.cpp" />' 'Compiled Visual Progress test registration'

    $smoke = $source.Smoke
    foreach ($required in @(
        "'9;4;1;0'",
        "'9;4;1;1'",
        "'9;4;1;50'",
        "'9;4;1;99'",
        "'9;4;1;100'",
        "'9;4;1;80'",
        "'9;4;1;20'",
        "'9;4;4;65'",
        "'9;4;3'",
        "'9;4;2;65'",
        "'9;4;0'",
        "'133;D;0'",
        "'133;D;1'",
        'Docker Pull',
        'Docker BuildKit',
        "Provider = 'pip'",
        "Provider = 'Git'",
        "Provider = 'curl'",
        "Provider = 'wget'",
        'Saving to:',
        'sample.bin 50%[',
        "Provider = 'npm'",
        "Provider = 'pnpm'",
        "Provider = 'yarn'",
        "Provider = 'nvm'",
        "Provider = 'Maven'",
        "Provider = 'Gradle'",
        'Generic fallback',
        'active pane should emit sparks',
        'Reduced Motion',
        'High Contrast',
        '[ValidateRange(0, 10000)]',
        'SoakIterations',
        'synthetic summary; must remain visible',
        'long-running command one-shot launch',
        'alternate screen during a long-running command',
        "Send-ControlSequence '[?1049h'",
        "Send-ControlSequence '[?1049l'",
        'launch animation plays one traversal',
        'expired launch fallback must not reappear',
        'appears at most once',
        'completion supersedes the launch animation immediately',
        'No files or external commands were used.'
    ))
    {
        Assert-Contains $smoke $required 'Manual Visual Progress Phase 2 fixture'
    }
    Assert-NotMatches $smoke '(?im)^\s*(?:docker|python|pip|git|curl(?:\.exe)?|wget(?:\.exe)?|npm|pnpm|yarn|nvm|mvn|gradlew)\b' 'Dependency-free synthetic smoke fixture'
    Assert-NotMatches $smoke '(?i)Out-File|Set-Content|Add-Content|Export-[A-Za-z]+|Invoke-WebRequest|Start-Process|Invoke-Expression' 'Artifact-free synthetic smoke fixture'

    $phase2Doc = $source.Phase2Doc
    foreach ($required in @(
        '# Visual Progress Phase 2',
        'Rainbow Arc Weld renderer',
        'one-element UI mailbox',
        'The composition layers',
        '8 live sparks per active pane',
        '24 live sparks globally',
        'There is no CPU-driven frame loop',
        'Reduced Motion and High Contrast',
        'Bounded CLI recognition',
        'try-lock-and-drop behavior',
        'XAML-Islands host',
        'HWND-derived visible/focused pair',
        'rebinds rainbow and comet movement',
        'light-surface palettes',
        'no meaningful progress value',
        'Docker Pull',
        'Docker BuildKit',
        'Generic fallback',
        'Modern Rich pip',
        'Maven Resolver output is not always prefixed',
        'transfer speed plus ETA',
        'cannot replace an active high-confidence built-in provider',
        'clears the Generic overlay immediately',
        'visualProgress.replaceRecognizedOutput',
        'defaults to `false`',
        'pip, Git, curl, or wget',
        'generic output is never suppressible',
        'alternate-screen output',
        'lifecycle-generation changes',
        '## Bounded shell launch fallback',
        'bounded launch indication',
        'not intended to represent the entire execution',
        'launch clock',
        'command-generation-scoped state',
        'cannot resurrect the expired',
        'does not upload or persist terminal content',
        'WINTERM_DISABLE_VISUAL_PROGRESS=1',
        'invoke-visual-progress-smoke.ps1',
        'SoakIterations 1000',
        'Phase 3',
        'visualProgress.enabled',
        'visualProgress.recognizeCliProgress',
        'visualProgress.performanceMode',
        '1.2.0'
    ))
    {
        Assert-Contains $phase2Doc $required 'Visual Progress Phase 2 developer documentation'
    }
    Assert-NotContains $phase2Doc 'version remain 1.1.3' 'Retired Phase 2-only version assumption'
    Assert-Contains $source.Phase1Doc '[Visual Progress Phase 2](visual-progress-phase2.md)' 'Phase 1 documentation hand-off'

    $releaseNotes = Get-RequiredContent -Root $root -RelativePath 'docs\releases\1.2.0.md'
    foreach ($requiredPattern in @(
        '(?is)winTerm\s+1\.2\.0',
        '(?is)Visual Progress',
        '(?is)visualProgress\.enabled',
        '(?is)visualProgress\.recognizeCliProgress',
        '(?is)visualProgress\.performanceMode',
        '(?is)visualProgress\.replaceRecognizedOutput',
        '(?is)Automatic',
        '(?is)Full',
        '(?is)Balanced',
        '(?is)Minimal',
        '(?is)High Contrast',
        '(?is)Reduced Motion',
        '(?is)accessib',
        '(?is)local.*bounded.*recognition|bounded.*recognition.*local',
        '(?is)WINTERM_DISABLE_VISUAL_PROGRESS=1',
        '(?is)SHA256SUMS\.txt',
        '(?is)SBOM',
        '(?is)not Authenticode-signed|unsigned'
    ))
    {
        Assert-Matches $releaseNotes $requiredPattern 'winTerm 1.2.0 Visual Progress release notes'
    }
    Assert-Matches $source.Changelog '(?m)^##\s+.*1\.2\.0' 'Changelog 1.2.0 entry'
    Assert-Matches $source.CurrentProgress '(?i)1\.2\.0.*Visual Progress|Visual Progress.*1\.2\.0' 'Current progress v1.2.0 milestone'
    Assert-Contains $source.Readme '/releases/latest' 'Stable latest-release README download route'
    Assert-Matches $source.Readme '(?i)1\.2\.0' 'README v1.2.0 surface'
    foreach ($requiredPattern in @(
        '(?is)recognition.*runs locally|locally.*recognition',
        '(?is)bounded.*newly\s+arriving\s+output|newly\s+arriving.*bounded',
        '(?is)scrollback.*not scanned|does not scan.*scrollback',
        '(?is)not uploaded|does not upload',
        '(?is)not persisted|does not persist',
        '(?is)not logged|does not log',
        '(?is)no Visual Progress telemetry|does not emit.*telemetry|emits?\s+no\s+Visual Progress\s+telemetry',
        '(?is)disabling CLI recognition.*inspection|recognition.*disabled.*inspection',
        '(?is)WINTERM_DISABLE_VISUAL_PROGRESS'
    ))
    {
        Assert-Matches $source.Privacy $requiredPattern 'Visual Progress local-recognition privacy disclosure'
    }

    $version = $source.VersionMetadata | ConvertFrom-Json
    $expectedVersionValues = [ordered]@{
        applicationVersion = '1.3.0-beta3'
        packageVersion = '1.3.0.6'
        moduleVersion = '1.3.0'
        modulePrerelease = 'beta3'
        channel = 'beta'
        tag = 'v1.3.0-beta3'
        workspaceSchemaVersion = 2
        dockingModelVersion = 1
        shellProtocolVersion = 1
        themeSchemaVersion = 1
        updateManifestSchemaVersion = 1
    }
    foreach ($entry in $expectedVersionValues.GetEnumerator())
    {
        if ($version.($entry.Key) -ne $entry.Value)
        {
            throw "Version metadata '$($entry.Key)' is '$($version.($entry.Key))', expected '$($entry.Value)'."
        }
    }
    $shellVersion = $source.ShellVersion | ConvertFrom-Json
    if ($shellVersion.applicationVersion -ne '1.3.0-beta3' -or $shellVersion.moduleVersion -ne '1.3.0' -or $shellVersion.protocolVersion -ne 1)
    {
        throw 'Shell version metadata does not match winTerm release 1.3.0-beta3 with protocol version 1.'
    }
    foreach ($surface in @(
        @{ Content = $source.ReleaseMetadata; Value = 'ApplicationVersion{ L"1.3.0-beta3" }'; Description = 'About release metadata' },
        @{ Content = $source.PackageManifest; Value = 'Version="1.3.0.6"'; Description = 'MSIX package manifest' },
        @{ Content = $source.HostResource; Value = 'FILEVERSION 1,3,0,6'; Description = 'Terminal host file version' },
        @{ Content = $source.HostResource; Value = '"ProductVersion", "1.3.0-beta3\0"'; Description = 'Terminal host display version' },
        @{ Content = $source.ShimResource; Value = 'FILEVERSION 1,3,0,6'; Description = 'Shim file version' },
        @{ Content = $source.ShimResource; Value = '"ProductVersion", "1.3.0-beta3\0"'; Description = 'Shim display version' },
        @{ Content = $source.CustomProps; Value = '<VersionMajor>1</VersionMajor>'; Description = 'Executable major version' },
        @{ Content = $source.CustomProps; Value = '<VersionMinor>3</VersionMinor>'; Description = 'Executable minor version' },
        @{ Content = $source.ShellModuleManifest; Value = "ModuleVersion = '1.3.0'"; Description = 'PowerShell module manifest' },
        @{ Content = $source.ShellModule; Value = "`$script:WinTermModuleVersion = '1.3.0'"; Description = 'PowerShell module runtime' },
        @{ Content = $source.PackageShellAssets; Value = "'shell\shared\version.json'"; Description = 'Canonical shell version metadata packaging' },
        @{ Content = $source.WorkspaceSerializer; Value = '"1.3.0-beta3"'; Description = 'Workspace application-version fallback' }
    ))
    {
        Assert-Contains $surface.Content $surface.Value $surface.Description
    }
    foreach ($required in @(
        "applicationVersion -eq '1.3.0-beta3'",
        "packageVersion -eq '1.3.0.6'",
        "moduleVersion -eq '1.3.0'",
        "tag -eq 'v1.3.0-beta3'",
        "Workspace Schema version remains 2",
        "Docking Model version remains 1",
        "Shell Protocol version remains 1",
        "Theme Schema remains at version 1"
    ))
    {
        Assert-Contains $source.VerifyVersion $required 'Authoritative v1.3.0-beta3 version validation surface'
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

    Write-Host 'PASS: Visual Progress Phase 3 settings, governor, accessibility, recognition, lifecycle, privacy, stress, documentation, and version boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Visual Progress tests failed: $($_.Exception.Message)"
    exit 1
}
