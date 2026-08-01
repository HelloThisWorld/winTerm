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
        throw "Visual Progress Phase 2 boundary '$RelativePath' is missing."
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
        Renderer = 'src\winterm\VisualProgress\RainbowArcRenderer.h'
        Recognition = 'src\winterm\VisualProgress\ProgressRecognition.h'
        Tests = 'src\cascadia\UnitTests_SettingsModel\WinTermVisualProgressTests.cpp'
        TestProject = 'src\cascadia\UnitTests_SettingsModel\SettingsModel.UnitTests.vcxproj'
        PaneCpp = 'src\cascadia\TerminalApp\Pane.cpp'
        PaneH = 'src\cascadia\TerminalApp\Pane.h'
        TerminalPageCpp = 'src\cascadia\TerminalApp\TerminalPage.cpp'
        AppXaml = 'src\cascadia\TerminalApp\App.xaml'
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
        Dispatch = 'src\terminal\adapter\adaptDispatch.cpp'
        Phase1Doc = 'docs\development\visual-progress-phase1.md'
        Phase2Doc = 'docs\development\visual-progress-phase2.md'
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
        'void Close() noexcept',
        'inline static SparkBudget _sharedSparkBudget'
    ))
    {
        Assert-Contains $renderer $required 'Dedicated Rainbow Arc composition renderer'
    }
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

    $pane = $source.PaneCpp
    foreach ($required in @(
        'WINTERM_DISABLE_VISUAL_PROGRESS',
        'RainbowArcVisualConstants::OverlayHostHeight',
        '_visualProgressOverlay.IsHitTestVisible(false)',
        'Controls::Grid::SetRow(_visualProgressOverlay, 1)',
        'AccessibilityView::Raw',
        'RainbowArcRenderer::TryCreate(_visualProgressCompositionHost)',
        'const auto rendererReady = _visualProgressRenderer && !_visualProgressRenderer->Faulted()',
        '_visualProgressRendererReady.store(rendererReady',
        '_visualProgressRenderer->SetPaneActive(_lastActive)',
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
    Assert-Matches $pane '(?s)const auto enabled = _visualProgressEnabled\.load\([^;]+&&\s*!_visualProgressFaulted\.load\([^;]+&&\s*_visualProgressRendererReady\.load\([^;]+;\s*terminalContent\.GetTermControl\(\)\.ConfigureVisualProgressRecognition\(\s*enabled,\s*enabled &&' 'Actual renderer-readiness suppression gate'
    Assert-NotContains $pane '_visualProgressOverlay.Height(6.0)' 'Centralized overlay geometry boundary'
    Assert-NotContains $pane '_visualProgressOverlay.RowDefinitions' 'Viewport-preserving overlay boundary'
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
        '== recognitionGeneration',
        '_visualProgressRecognitionResetRequested',
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

    Assert-Contains $source.Settings 'VisualProgressEnabled, "visualProgress.enabled", false' 'Visual Progress default-off setting'
    Assert-Contains $source.Settings 'VisualProgressReplaceRecognizedOutput, "visualProgress.replaceRecognizedOutput", false' 'Replacement preview default-off setting'
    Assert-Contains $source.SettingsIdl 'INHERITABLE_SETTING(Boolean, VisualProgressEnabled)' 'Visual Progress setting projection'
    Assert-Contains $source.SettingsIdl 'INHERITABLE_SETTING(Boolean, VisualProgressReplaceRecognizedOutput)' 'Replacement preview setting projection'

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
        'SettingSerializesAndMissingSettingDefaultsOff',
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
        'visualProgress.replaceRecognizedOutput'
    ))
    {
        Assert-Contains $tests $required 'Visual Progress Phase 2 compiled test coverage'
    }
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
        'does not upload or persist terminal content',
        'WINTERM_DISABLE_VISUAL_PROGRESS=1',
        'invoke-visual-progress-smoke.ps1',
        'SoakIterations 1000',
        'Phase 3',
        'version remain 1.1.3'
    ))
    {
        Assert-Contains $phase2Doc $required 'Visual Progress Phase 2 developer documentation'
    }
    Assert-Contains $source.Phase1Doc '[Visual Progress Phase 2](visual-progress-phase2.md)' 'Phase 1 documentation hand-off'

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

    Write-Host 'PASS: Visual Progress Phase 2 renderer, recognition, lifecycle, privacy, and safety boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Visual Progress tests failed: $($_.Exception.Message)"
    exit 1
}
