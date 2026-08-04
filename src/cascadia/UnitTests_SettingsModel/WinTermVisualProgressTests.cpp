// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/GlobalAppSettings.h"
#include "../../winterm/VisualProgress/ProgressRecognition.h"
#include "../../winterm/VisualProgress/VisualProgressAccessibility.h"
#include "../../winterm/VisualProgress/VisualProgressModel.h"
#include "../../winterm/VisualProgress/VisualProgressPerformanceGovernor.h"
#include "../../winterm/VisualProgress/VisualProgressRenderModel.h"
#include "../../winterm/VisualProgress/VisualProgressSamplerState.h"

#include <thread>

using namespace WEX::TestExecution;
using namespace winTerm::VisualProgress;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace SettingsModelUnitTests
{
    class WinTermVisualProgressTests
    {
        TEST_CLASS(WinTermVisualProgressTests);

        TEST_METHOD(MapEveryTaskbarState);
        TEST_METHOD(ClampDeterminateValues);
        TEST_METHOD(SuppressDuplicateState);
        TEST_METHOD(StandardProgressPrecedesShellLifecycle);
        TEST_METHOD(ProviderProgressPrecedesShellLifecycle);
        TEST_METHOD(StandardProgressPrecedesProviderAndFallsBack);
        TEST_METHOD(ProviderStatePackingContainsOnlyStructuralFields);
        TEST_METHOD(RecognitionClassifiesProvidersAndGenericFallback);
        TEST_METHOD(RecognitionHandlesFragmentationAndMalformedInput);
        TEST_METHOD(RecognitionSuppressesOnlySafeWholeChunks);
        TEST_METHOD(RecognitionBoundsStateAndCoalescesUpdates);
        TEST_METHOD(RecognitionKeepsOverlayOnlyProvidersVisible);
        TEST_METHOD(RecognitionPreservesTerminalTextAndTerminalStates);
        TEST_METHOD(RecognitionTerminalFailuresClearProviderContext);
        TEST_METHOD(RecognitionRecoversAndIsolatesEngines);
        TEST_METHOD(RecognitionClassifiesAllProvidersOneCodeUnitAtATime);
        TEST_METHOD(RecognitionRejectsMalformedNumericAndInterruptedOutput);
        TEST_METHOD(RecognitionPreservesHighConfidenceOwnershipAndClearsGeneric);
        TEST_METHOD(RecognitionIgnoresProductMentionsInListings);
        TEST_METHOD(RecognitionClearsStaleRunningProviderAfterOrdinaryRecords);
        TEST_METHOD(RecognitionBootstrapsRichPipAndMavenResolver);
        TEST_METHOD(RecognitionHandlesGenericIndeterminateShapes);
        TEST_METHOD(RecognitionHandlesArbitraryProviderSplitsAndReset);
        TEST_METHOD(RecognitionAppliesSuppressionSafetyMatrix);
        TEST_METHOD(RendererPlansRealValuesRegressionAndIndeterminateMode);
        TEST_METHOD(RendererHiddenIngressAndOwnershipBoundaries);
        TEST_METHOD(RendererPlansStatusAndAccessibilityFallbacks);
        TEST_METHOD(RendererFailureAndCloseRemainPaneLocal);
        TEST_METHOD(SparkPoolsEnforcePaneAndGlobalCaps);
        TEST_METHOD(BackgroundAndHiddenPanesDoNotRequestSparkWork);
        TEST_METHOD(CommandCompletionClearsAtNextPrompt);
        TEST_METHOD(EmergencyOverridePrecedesSetting);
        TEST_METHOD(DisabledFeatureIgnoresEvents);
        TEST_METHOD(MultiplePanesRemainIndependent);
        TEST_METHOD(CloseAndDetachCleanupStopsUpdates);
        TEST_METHOD(SplitOrDetachResetClearsReusableState);
        TEST_METHOD(FeatureReloadDisablesAndReenablesCleanly);
        TEST_METHOD(MailboxCoalescesRapidUpdatesAndReleasesOnClose);
        TEST_METHOD(UnexpectedEventsFailOpen);
        TEST_METHOD(SettingsUseStableDefaultsAndRoundTrip);
        TEST_METHOD(GovernorAppliesModesAndEnvironmentCaps);
        TEST_METHOD(GovernorHysteresisIsBoundedAndDeterministic);
        TEST_METHOD(AccessibilitySemanticsAndAnnouncementsAreBounded);
        TEST_METHOD(Phase3StressCoverageRemainsBounded);
    };

    void WinTermVisualProgressTests::MapEveryTaskbarState()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);

        auto snapshot = state.ApplyTaskbar(1, 50);
        VERIFY_IS_TRUE(snapshot.has_value());
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Determinate), static_cast<int>(snapshot->mode));
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Running), static_cast<int>(snapshot->status));
        VERIFY_ARE_EQUAL(uint8_t{ 50 }, snapshot->value);

        snapshot = state.ApplyTaskbar(3, 0);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(snapshot->mode));

        snapshot = state.ApplyTaskbar(4, 27);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Waiting), static_cast<int>(snapshot->status));
        VERIFY_ARE_EQUAL(uint8_t{ 27 }, snapshot->value);

        snapshot = state.ApplyTaskbar(2, 0);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Error), static_cast<int>(snapshot->status));
        VERIFY_ARE_EQUAL(uint8_t{ 27 }, snapshot->value);

        snapshot = state.ApplyTaskbar(0, 0);
        VERIFY_IS_TRUE(snapshot.has_value());
        VERIFY_IS_FALSE(snapshot->visible);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Hidden), static_cast<int>(snapshot->mode));
    }

    void WinTermVisualProgressTests::ClampDeterminateValues()
    {
        for (const auto value : { uint64_t{ 0 }, uint64_t{ 1 }, uint64_t{ 50 }, uint64_t{ 99 }, uint64_t{ 100 }, uint64_t{ 101 }, uint64_t{ 1000 } })
        {
            ProgressStateMachine state;
            state.SetEnabled(true);
            const auto snapshot = state.ApplyTaskbar(1, value);
            VERIFY_IS_TRUE(snapshot.has_value());
            VERIFY_ARE_EQUAL(static_cast<uint8_t>(std::min<uint64_t>(value, 100)), snapshot->value);
        }
    }

    void WinTermVisualProgressTests::SuppressDuplicateState()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        VERIFY_IS_TRUE(state.ApplyTaskbar(1, 50).has_value());
        VERIFY_IS_FALSE(state.ApplyTaskbar(1, 50).has_value());
        VERIFY_IS_TRUE(state.ApplyTaskbar(1, 51).has_value());
    }

    void WinTermVisualProgressTests::StandardProgressPrecedesShellLifecycle()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1);
        const auto explicitProgress = state.ApplyTaskbar(1, 40);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressSource::Taskbar), static_cast<int>(explicitProgress->source));
        VERIFY_IS_FALSE(state.ApplyShellLifecycle(ShellLifecycleState::CommandFinished, 1).has_value());

        const auto fallback = state.ApplyTaskbar(0, 0);
        VERIFY_IS_TRUE(fallback.has_value());
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressSource::ShellIntegration), static_cast<int>(fallback->source));
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Error), static_cast<int>(fallback->status));
    }

    void WinTermVisualProgressTests::ProviderProgressPrecedesShellLifecycle()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1);

        const ProviderProgress provider{
            ProgressProvider::Git,
            ProgressMode::Determinate,
            ProgressStatus::Running,
            42,
            ProviderConfidence::High,
            true,
            true,
            true,
            2,
            7,
        };
        const auto snapshot = state.ApplyProvider(provider);
        VERIFY_IS_TRUE(snapshot.has_value());
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressSource::Provider), static_cast<int>(snapshot->source));
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Git), static_cast<int>(snapshot->provider));
        VERIFY_ARE_EQUAL(uint8_t{ 42 }, snapshot->value);

        VERIFY_IS_FALSE(state.ApplyShellLifecycle(ShellLifecycleState::CommandExecuted, -1).has_value());
        const auto fallback = state.ResetProvider();
        VERIFY_IS_TRUE(fallback.has_value());
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressSource::ShellIntegration), static_cast<int>(fallback->source));
    }

    void WinTermVisualProgressTests::StandardProgressPrecedesProviderAndFallsBack()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1);
        state.ApplyProvider({
            ProgressProvider::Pip,
            ProgressMode::Determinate,
            ProgressStatus::Running,
            25,
            ProviderConfidence::High,
            true,
            true,
            true,
            1,
            1,
        });

        const auto explicitProgress = state.ApplyTaskbar(1, 75);
        VERIFY_IS_TRUE(explicitProgress.has_value());
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressSource::Taskbar), static_cast<int>(explicitProgress->source));

        VERIFY_IS_FALSE(state.ApplyProvider({
                                                ProgressProvider::Pip,
                                                ProgressMode::Determinate,
                                                ProgressStatus::Running,
                                                50,
                                                ProviderConfidence::High,
                                                true,
                                                true,
                                                true,
                                                1,
                                                2,
                                            })
                            .has_value());

        const auto fallback = state.ApplyTaskbar(0, 0);
        VERIFY_IS_TRUE(fallback.has_value());
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressSource::Provider), static_cast<int>(fallback->source));
        VERIFY_ARE_EQUAL(uint8_t{ 50 }, fallback->value);

        const auto prompt = state.ApplyShellLifecycle(ShellLifecycleState::Prompt, -1);
        VERIFY_IS_TRUE(prompt.has_value());
        VERIFY_IS_FALSE(prompt->visible);
    }

    void WinTermVisualProgressTests::ProviderStatePackingContainsOnlyStructuralFields()
    {
        const ProviderProgress expected{
            ProgressProvider::Gradle,
            ProgressMode::Indeterminate,
            ProgressStatus::Waiting,
            99,
            ProviderConfidence::High,
            true,
            true,
            false,
            4095,
            123456,
        };
        const auto actual = UnpackProviderProgress(PackProviderProgress(expected));
        VERIFY_ARE_EQUAL(static_cast<int>(expected.provider), static_cast<int>(actual.provider));
        VERIFY_ARE_EQUAL(static_cast<int>(expected.mode), static_cast<int>(actual.mode));
        VERIFY_ARE_EQUAL(static_cast<int>(expected.status), static_cast<int>(actual.status));
        VERIFY_ARE_EQUAL(expected.value, actual.value);
        VERIFY_ARE_EQUAL(static_cast<int>(expected.confidence), static_cast<int>(actual.confidence));
        VERIFY_ARE_EQUAL(expected.visible, actual.visible);
        VERIFY_ARE_EQUAL(expected.transient, actual.transient);
        VERIFY_ARE_EQUAL(expected.suppressible, actual.suppressible);
        VERIFY_ARE_EQUAL(expected.stage, actual.stage);
        VERIFY_ARE_EQUAL(expected.sequence, actual.sequence);
    }

    void WinTermVisualProgressTests::RecognitionClassifiesProvidersAndGenericFallback()
    {
        const auto expectProvider = [](const std::wstring_view line,
                                       const ProgressProvider provider,
                                       const uint8_t value) {
            RecognitionEngine engine;
            const auto result = engine.Consume(line, 0);
            VERIFY_IS_TRUE(result.progress.has_value());
            if (!result.progress)
            {
                return;
            }
            VERIFY_ARE_EQUAL(static_cast<int>(provider), static_cast<int>(result.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Determinate), static_cast<int>(result.progress->mode));
            VERIFY_ARE_EQUAL(value, result.progress->value);
            VERIFY_IS_FALSE(result.suppressInput);
        };

        expectProvider(L"demo-layer: Downloading 512B/1.0kB\n", ProgressProvider::DockerPull, 50);
        expectProvider(L"#7 [3/8] RUN build 50%\n", ProgressProvider::DockerBuildKit, 50);
        expectProvider(L"Downloading demo.whl 50% 512kB/1.0MB 1.0MB/s eta 00:01\n", ProgressProvider::Pip, 50);
        expectProvider(L"Receiving objects: 50% (50/100)\n", ProgressProvider::Git, 50);
        expectProvider(L"wget demo 50%[====>     ] 512K 1.0MB/s eta 1s\n", ProgressProvider::Wget, 50);
        expectProvider(L"npm fetch 50% (5/10)\n", ProgressProvider::Npm, 50);
        expectProvider(L"pnpm download 50% (5/10)\n", ProgressProvider::Pnpm, 50);
        expectProvider(L"yarn fetch 50% (5/10)\n", ProgressProvider::Yarn, 50);
        expectProvider(L"nvm downloading node.js 50%\n", ProgressProvider::Nvm, 50);
        expectProvider(L"[INFO] Progress (1): 512/1024 kB\n", ProgressProvider::Maven, 50);
        expectProvider(L"75% EXECUTING\n", ProgressProvider::Gradle, 75);
        expectProvider(L"42% (42/100) 1.0 MB/s ETA 00:05\n", ProgressProvider::Generic, 42);

        RecognitionEngine curl;
        curl.Consume(L"% Total    % Received\r\n", 0);
        const auto curlResult = curl.Consume(L"50  1024  50  512  0\r\x1b[2K", 50);
        VERIFY_IS_TRUE(curlResult.progress.has_value());
        if (curlResult.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Curl), static_cast<int>(curlResult.progress->provider));
            VERIFY_ARE_EQUAL(uint8_t{ 50 }, curlResult.progress->value);
        }

        RecognitionEngine unknownTransfer;
        const auto generic = unknownTransfer.Consume(L"42% (42/100) 1.0 MB/s ETA 00:05\n", 0);
        VERIFY_IS_TRUE(generic.progress.has_value());
        if (generic.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(generic.progress->provider));
            VERIFY_IS_FALSE(generic.progress->suppressible);
        }

        RecognitionEngine etaOnly;
        VERIFY_IS_FALSE(etaOnly.Consume(L"working ETA soon\n", 0).progress.has_value());

        RecognitionEngine prompt;
        const auto promptResult = prompt.Consume(L"continue? [y/n] 50% (1/2)\r\x1b[2K", 0);
        VERIFY_IS_FALSE(promptResult.progress.has_value());
        VERIFY_IS_FALSE(promptResult.suppressInput);
    }

    void WinTermVisualProgressTests::RecognitionHandlesFragmentationAndMalformedInput()
    {
        RecognitionEngine splitCrLf;
        const auto carriageReturn = splitCrLf.Consume(L"Receiving objects: 50% (50/100)\r", 0);
        VERIFY_IS_FALSE(carriageReturn.progress.has_value());
        VERIFY_IS_FALSE(carriageReturn.suppressInput);
        const auto lineFeed = splitCrLf.Consume(L"\n", 50);
        VERIFY_IS_TRUE(lineFeed.progress.has_value());
        if (lineFeed.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Git), static_cast<int>(lineFeed.progress->provider));
            VERIFY_IS_FALSE(lineFeed.progress->transient);
        }
        VERIFY_IS_FALSE(lineFeed.suppressInput);

        RecognitionEngine splitCsi;
        const auto csiPrefix = splitCsi.Consume(L"Receiving objects: 50% (50/100)\r\x1b[", 0);
        VERIFY_IS_TRUE(csiPrefix.progress.has_value());
        VERIFY_IS_FALSE(csiPrefix.suppressInput);
        const auto csiSuffix = splitCsi.Consume(L"2K", 50);
        VERIFY_IS_TRUE(csiSuffix.healthy);
        VERIFY_IS_FALSE(csiSuffix.suppressInput);

        RecognitionEngine splitSurrogate;
        std::wstring surrogatePrefix{ L"42% (42/100) ETA 00:01 " };
        surrogatePrefix.push_back(static_cast<wchar_t>(0xd83d));
        const auto high = splitSurrogate.Consume(surrogatePrefix, 0);
        VERIFY_IS_TRUE(high.healthy);
        VERIFY_IS_FALSE(high.progress.has_value());
        std::wstring surrogateSuffix;
        surrogateSuffix.push_back(static_cast<wchar_t>(0xde00));
        surrogateSuffix.append(L"\r\x1b[2K");
        const auto low = splitSurrogate.Consume(surrogateSuffix, 50);
        VERIFY_IS_TRUE(low.healthy);
        VERIFY_IS_TRUE(low.progress.has_value());
        if (low.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(low.progress->provider));
        }
        VERIFY_IS_FALSE(low.suppressInput);

        RecognitionEngine malformedUtf16;
        std::wstring malformedLine;
        malformedLine.push_back(static_cast<wchar_t>(0xdc00));
        malformedLine.append(L"42% (42/100) ETA 00:01\r\x1b[2K");
        const auto malformedResult = malformedUtf16.Consume(malformedLine, 0);
        VERIFY_IS_FALSE(malformedResult.healthy);
        VERIFY_IS_FALSE(malformedResult.progress.has_value());
        VERIFY_IS_FALSE(malformedResult.suppressInput);

        RecognitionEngine utf16Engine;
        Utf8RecognitionAdapter utf8{ utf16Engine };
        const std::string bytes = "42% (42/100) 1.0 MB/s ETA 00:05 \xf0\x9f\x98\x80\r\x1b[2K";
        std::optional<ProviderProgress> latest;
        for (size_t i = 0; i < bytes.size(); ++i)
        {
            const auto result = utf8.Consume(std::string_view{ bytes.data() + i, 1 }, i * 50);
            VERIFY_IS_TRUE(result.accepted);
            VERIFY_IS_TRUE(result.healthy);
            VERIFY_IS_FALSE(result.suppressInput);
            if (result.progress)
            {
                latest = result.progress;
            }
        }
        VERIFY_IS_TRUE(latest.has_value());
        if (latest)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(latest->provider));
        }
        VERIFY_IS_TRUE(utf8.Finish().healthy);

        RecognitionEngine malformedUtf8Engine;
        Utf8RecognitionAdapter malformedUtf8{ malformedUtf8Engine };
        const auto invalid = malformedUtf8.Consume(std::string_view{ "\xc0\x80", 2 }, 0);
        VERIFY_IS_FALSE(invalid.accepted);
        VERIFY_IS_FALSE(invalid.healthy);
        VERIFY_IS_FALSE(invalid.suppressInput);
        VERIFY_IS_FALSE(malformedUtf8.Consume("42% (42/100) ETA\n", 50).accepted);
        malformedUtf8.Reset();
        VERIFY_IS_TRUE(malformedUtf8.Consume("42% (42/100) ETA\n", 100).progress.has_value());

        RecognitionEngine truncatedUtf8Engine;
        Utf8RecognitionAdapter truncatedUtf8{ truncatedUtf8Engine };
        VERIFY_IS_TRUE(truncatedUtf8.Consume(std::string_view{ "\xe2", 1 }, 0).healthy);
        const auto truncated = truncatedUtf8.Finish();
        VERIFY_IS_FALSE(truncated.accepted);
        VERIFY_IS_FALSE(truncated.healthy);
        VERIFY_IS_FALSE(truncated.suppressInput);
    }

    void WinTermVisualProgressTests::RecognitionSuppressesOnlySafeWholeChunks()
    {
        RecognitionOptions replacement;
        replacement.replacementEnabled = true;
        replacement.rendererEnabled = true;

        const auto primeCursor = [](RecognitionEngine& engine) {
            const auto result = engine.Consume(L"\r", 0);
            VERIFY_IS_FALSE(result.suppressInput);
        };

        RecognitionEngine git;
        primeCursor(git);
        const auto gitResult = git.Consume(L"Receiving objects: 50% (50/100)\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(gitResult.progress.has_value());
        VERIFY_IS_TRUE(gitResult.suppressInput);

        RecognitionEngine pip;
        primeCursor(pip);
        const auto pipResult = pip.Consume(L"Downloading demo.whl 50% 512kB/1.0MB 1.0MB/s eta 00:01\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(pipResult.progress.has_value());
        VERIFY_IS_TRUE(pipResult.suppressInput);

        RecognitionEngine unrelatedDownload;
        primeCursor(unrelatedDownload);
        const auto backupResult = unrelatedDownload.Consume(L"Downloading backup.zip 50% 1MB/2MB 1MB/s eta 1s\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(backupResult.progress.has_value());
        if (backupResult.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(backupResult.progress->provider));
            VERIFY_IS_FALSE(backupResult.progress->suppressible);
        }
        VERIFY_IS_FALSE(backupResult.suppressInput);

        RecognitionEngine wget;
        wget.Consume(L"Saving to: 'demo.bin'\n", 0);
        primeCursor(wget);
        const auto wgetResult = wget.Consume(L"demo.bin 50%[====>     ] 512K 1.0MB/s eta 1s\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(wgetResult.progress.has_value());
        VERIFY_IS_TRUE(wgetResult.suppressInput);

        RecognitionEngine unrelatedBracketMeter;
        primeCursor(unrelatedBracketMeter);
        const auto bracketResult = unrelatedBracketMeter.Consume(L"Backup 50%[====>     ] 1MB/s eta 1s\r\x1b[2K", 50, replacement);
        VERIFY_IS_FALSE(bracketResult.progress.has_value());
        VERIFY_IS_FALSE(bracketResult.suppressInput);

        RecognitionEngine curl;
        curl.Consume(L"% Total    % Received\r\n", 0);
        const auto curlResult = curl.Consume(L"50  1024  50  512  0\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(curlResult.progress.has_value());
        VERIFY_IS_TRUE(curlResult.suppressInput);

        RecognitionEngine generic;
        primeCursor(generic);
        const auto genericResult = generic.Consume(L"42% (42/100) 1.0 MB/s ETA 00:05\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(genericResult.progress.has_value());
        VERIFY_IS_FALSE(genericResult.suppressInput);

        const auto expectGitPreserved = [&](const RecognitionOptions options) {
            RecognitionEngine engine;
            primeCursor(engine);
            const auto result = engine.Consume(L"Receiving objects: 50% (50/100)\r\x1b[2K", 50, options);
            VERIFY_IS_TRUE(result.progress.has_value());
            VERIFY_IS_FALSE(result.suppressInput);
        };

        auto gated = replacement;
        gated.replacementEnabled = false;
        expectGitPreserved(gated);
        gated = replacement;
        gated.rendererEnabled = false;
        expectGitPreserved(gated);
        gated = replacement;
        gated.normalScreen = false;
        expectGitPreserved(gated);
        gated = replacement;
        gated.parserHealthy = false;
        expectGitPreserved(gated);

        RecognitionEngine resetOrigin;
        primeCursor(resetOrigin);
        resetOrigin.Reset();
        VERIFY_IS_FALSE(resetOrigin.Consume(L"Receiving objects: 50% (50/100)\r\x1b[2K", 50, replacement).suppressInput);
        primeCursor(resetOrigin);
        VERIFY_IS_TRUE(resetOrigin.Consume(L"Receiving objects: 60% (60/100)\r\x1b[2K", 100, replacement).suppressInput);

        RecognitionEngine newline;
        primeCursor(newline);
        VERIFY_IS_FALSE(newline.Consume(L"Receiving objects: 50% (50/100)\n", 50, replacement).suppressInput);

        RecognitionEngine ambiguousCarriageReturn;
        primeCursor(ambiguousCarriageReturn);
        VERIFY_IS_FALSE(ambiguousCarriageReturn.Consume(L"Receiving objects: 50% (50/100)\r", 50, replacement).suppressInput);

        RecognitionEngine fragmented;
        primeCursor(fragmented);
        VERIFY_IS_FALSE(fragmented.Consume(L"Receiving objects: 50% ", 50, replacement).suppressInput);
        VERIFY_IS_FALSE(fragmented.Consume(L"(50/100)\r\x1b[2K", 100, replacement).suppressInput);

        RecognitionEngine styled;
        primeCursor(styled);
        const auto sgr = styled.Consume(L"\x1b[31mReceiving objects: 50% (50/100)\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(sgr.progress.has_value());
        VERIFY_IS_FALSE(sgr.suppressInput);

        RecognitionEngine unsupportedCsi;
        primeCursor(unsupportedCsi);
        const auto cursorControl = unsupportedCsi.Consume(L"\x1b[?25hReceiving objects: 50% (50/100)\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(cursorControl.progress.has_value());
        VERIFY_IS_FALSE(cursorControl.suppressInput);

        RecognitionEngine warning;
        primeCursor(warning);
        const auto warningResult = warning.Consume(L"Downloading demo.whl 50% 512kB/1.0MB 1.0MB/s warning\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(warningResult.progress.has_value());
        VERIFY_IS_FALSE(warningResult.suppressInput);

        RecognitionEngine prompt;
        primeCursor(prompt);
        const auto promptResult = prompt.Consume(L"username 50% (1/2)\r\x1b[2K", 50, replacement);
        VERIFY_IS_FALSE(promptResult.progress.has_value());
        VERIFY_IS_FALSE(promptResult.suppressInput);
    }

    void WinTermVisualProgressTests::RecognitionBoundsStateAndCoalescesUpdates()
    {
        RecognitionEngine chunkBound;
        std::wstring oversizedChunk(RecognitionEngine::MaxChunkCodeUnits + 1, L'a');
        const auto chunkOverflow = chunkBound.Consume(oversizedChunk, 0);
        VERIFY_IS_TRUE(chunkOverflow.overflow);
        VERIFY_IS_FALSE(chunkOverflow.healthy);
        VERIFY_IS_FALSE(chunkOverflow.suppressInput);

        RecognitionEngine recordBound;
        std::wstring oversizedRecord(RecognitionEngine::MaxCurrentLineCodeUnits + 1, L'a');
        oversizedRecord.push_back(L'\n');
        const auto recordOverflow = recordBound.Consume(oversizedRecord, 0);
        VERIFY_IS_TRUE(recordOverflow.overflow);
        VERIFY_IS_FALSE(recordOverflow.healthy);
        VERIFY_IS_FALSE(recordOverflow.progress.has_value());
        const auto recordRecovered = recordBound.Consume(L"42% (42/100) ETA 00:01\n", 50);
        VERIFY_IS_TRUE(recordRecovered.progress.has_value());

        RecognitionEngine ansiBound;
        std::wstring oversizedCsi{ L"\x1b[" };
        oversizedCsi.append(RecognitionEngine::MaxAnsiSequenceCodeUnits + 1, L'1');
        oversizedCsi.push_back(L'K');
        const auto ansiOverflow = ansiBound.Consume(oversizedCsi, 0);
        VERIFY_IS_TRUE(ansiOverflow.overflow);
        VERIFY_IS_FALSE(ansiOverflow.healthy);
        VERIFY_IS_FALSE(ansiOverflow.suppressInput);

        RecognitionEngine dockerBound;
        std::wstring dockerLines;
        for (size_t i = 0; i <= RecognitionEngine::DockerLayerCapacity; ++i)
        {
            dockerLines.append(L"layer");
            dockerLines.append(std::to_wstring(i));
            dockerLines.append(L": Downloading 1B/2B\n");
        }
        const auto dockerOverflow = dockerBound.Consume(dockerLines, 0);
        VERIFY_IS_TRUE(dockerOverflow.overflow);
        VERIFY_IS_FALSE(dockerOverflow.healthy);
        VERIFY_IS_FALSE(dockerOverflow.suppressInput);

        RecognitionEngine buildKitBound;
        std::wstring buildKitLines;
        for (size_t i = 1; i <= RecognitionEngine::BuildKitStepCapacity + 1; ++i)
        {
            buildKitLines.push_back(L'#');
            buildKitLines.append(std::to_wstring(i));
            buildKitLines.append(L" RUN 50%\n");
        }
        const auto buildKitOverflow = buildKitBound.Consume(buildKitLines, 0);
        VERIFY_IS_TRUE(buildKitOverflow.overflow);
        VERIFY_IS_FALSE(buildKitOverflow.healthy);

        RecognitionEngine coalesced;
        const auto first = coalesced.Consume(L"Receiving objects: 10% (10/100)\n", 0);
        VERIFY_IS_TRUE(first.progress.has_value());
        VERIFY_IS_FALSE(coalesced.Consume(L"Receiving objects: 20% (20/100)\n", 10).progress.has_value());
        VERIFY_IS_FALSE(coalesced.Consume(L"Receiving objects: 30% (30/100)\n", 49).progress.has_value());
        const auto due = coalesced.Consume(L"Receiving objects: 40% (40/100)\n", 50);
        VERIFY_IS_TRUE(due.progress.has_value());
        if (due.progress)
        {
            VERIFY_ARE_EQUAL(uint8_t{ 40 }, due.progress->value);
        }

        RecognitionEngine newestOnly;
        const auto newest = newestOnly.Consume(
            L"Receiving objects: 10% (10/100)\rReceiving objects: 20% (20/100)\r\n",
            0);
        VERIFY_IS_TRUE(newest.progress.has_value());
        if (newest.progress)
        {
            VERIFY_ARE_EQUAL(uint8_t{ 20 }, newest.progress->value);
        }

        RecognitionEngine stress;
        bool stressHealthy{ true };
        for (size_t i = 0; i < 10000; ++i)
        {
            const auto value = i % 101;
            std::wstring line{ L"Receiving objects: " };
            line.append(std::to_wstring(value));
            line.append(L"% (");
            line.append(std::to_wstring(value));
            line.append(L"/100)\n");
            const auto result = stress.Consume(line, i);
            stressHealthy = stressHealthy && result.accepted && result.healthy && !result.overflow;
        }
        VERIFY_IS_TRUE(stressHealthy);

        RecognitionEngine utf8Engine;
        Utf8RecognitionAdapter utf8{ utf8Engine };
        std::string oversizedUtf8(Utf8RecognitionAdapter::MaxChunkBytes + 1, 'a');
        const auto utf8Overflow = utf8.Consume(oversizedUtf8, 0);
        VERIFY_IS_TRUE(utf8Overflow.overflow);
        VERIFY_IS_FALSE(utf8Overflow.accepted);
        VERIFY_IS_FALSE(utf8Overflow.healthy);
        VERIFY_IS_FALSE(utf8Overflow.suppressInput);
    }

    void WinTermVisualProgressTests::RecognitionKeepsOverlayOnlyProvidersVisible()
    {
        RecognitionOptions replacement;
        replacement.replacementEnabled = true;
        replacement.rendererEnabled = true;

        const auto expectOverlayOnly = [&](const std::wstring_view line, const ProgressProvider provider) {
            RecognitionEngine engine;
            engine.Consume(L"\r", 0);
            const auto result = engine.Consume(line, 50, replacement);
            VERIFY_IS_TRUE(result.progress.has_value());
            if (!result.progress)
            {
                return;
            }
            VERIFY_ARE_EQUAL(static_cast<int>(provider), static_cast<int>(result.progress->provider));
            VERIFY_IS_FALSE(result.progress->suppressible);
            VERIFY_IS_FALSE(result.suppressInput);
        };

        expectOverlayOnly(L"demo-layer: Downloading 512B/1.0kB\r\x1b[2K", ProgressProvider::DockerPull);
        expectOverlayOnly(L"#7 [3/8] RUN build 50%\r\x1b[2K", ProgressProvider::DockerBuildKit);
        expectOverlayOnly(L"npm fetch 50% (5/10)\r\x1b[2K", ProgressProvider::Npm);
        expectOverlayOnly(L"pnpm download 50% (5/10)\r\x1b[2K", ProgressProvider::Pnpm);
        expectOverlayOnly(L"yarn fetch 50% (5/10)\r\x1b[2K", ProgressProvider::Yarn);
        expectOverlayOnly(L"nvm downloading node.js 50%\r\x1b[2K", ProgressProvider::Nvm);
        expectOverlayOnly(L"[INFO] Progress (1): 512/1024 kB\r\x1b[2K", ProgressProvider::Maven);
        expectOverlayOnly(L"75% EXECUTING\r\x1b[2K", ProgressProvider::Gradle);
    }

    void WinTermVisualProgressTests::RecognitionPreservesTerminalTextAndTerminalStates()
    {
        RecognitionOptions replacement;
        replacement.replacementEnabled = true;
        replacement.rendererEnabled = true;

        const auto expectTerminal = [&](const std::wstring_view line,
                                        const ProgressProvider provider,
                                        const ProgressStatus status) {
            RecognitionEngine engine;
            engine.Consume(L"\r", 0);
            const auto result = engine.Consume(line, 50, replacement);
            VERIFY_IS_TRUE(result.progress.has_value());
            if (!result.progress)
            {
                return;
            }
            VERIFY_ARE_EQUAL(static_cast<int>(provider), static_cast<int>(result.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(status), static_cast<int>(result.progress->status));
            VERIFY_IS_FALSE(result.suppressInput);
        };

        expectTerminal(L"Error response from daemon: pull access denied\r\x1b[2K", ProgressProvider::DockerPull, ProgressStatus::Error);
        expectTerminal(L"Downloaded newer image for demo:latest\r\x1b[2K", ProgressProvider::DockerPull, ProgressStatus::Success);
        expectTerminal(L"fatal: authentication failed\r\x1b[2K", ProgressProvider::Git, ProgressStatus::Error);
        expectTerminal(L"npm ERR! failed to fetch package\r\x1b[2K", ProgressProvider::Npm, ProgressStatus::Error);
        expectTerminal(L"npm completed\r\x1b[2K", ProgressProvider::Npm, ProgressStatus::Success);
        expectTerminal(L"[INFO] BUILD SUCCESS\r\x1b[2K", ProgressProvider::Maven, ProgressStatus::Success);
        expectTerminal(L"[INFO] BUILD FAILURE\r\x1b[2K", ProgressProvider::Maven, ProgressStatus::Error);
        expectTerminal(L"BUILD SUCCESSFUL in 1s\r\x1b[2K", ProgressProvider::Gradle, ProgressStatus::Success);
        expectTerminal(L"BUILD FAILED in 1s\r\x1b[2K", ProgressProvider::Gradle, ProgressStatus::Error);
        expectTerminal(L"curl: (22) The requested URL returned error\r\x1b[2K", ProgressProvider::Curl, ProgressStatus::Error);
        expectTerminal(L"wget: unable to resolve host address\r\x1b[2K", ProgressProvider::Wget, ProgressStatus::Error);

        RecognitionEngine gitAuthentication;
        gitAuthentication.Consume(L"Receiving objects: 25% (25/100)\n", 0);
        const auto authenticationFailure = gitAuthentication.Consume(L"authentication failed for repository\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(authenticationFailure.progress.has_value());
        if (authenticationFailure.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Git), static_cast<int>(authenticationFailure.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Error), static_cast<int>(authenticationFailure.progress->status));
        }
        VERIFY_IS_FALSE(authenticationFailure.suppressInput);

        for (const auto line : {
                 L"npm WARN audit report 50% (1/2)\r\x1b[2K",
                 L"npm audit found 2 vulnerabilities 50% (1/2)\r\x1b[2K",
                 L"Authentication required: 50% (1/2)\r\x1b[2K",
                 L"Password: 50% (1/2)\r\x1b[2K",
                 L"Enter passphrase: 50% (1/2)\r\x1b[2K",
                 L"Confirm continue? [y/n] 50% (1/2)\r\x1b[2K",
                 L"Username: 50% (1/2)\r\x1b[2K" })
        {
            RecognitionEngine preserved;
            preserved.Consume(L"\r", 0);
            const auto result = preserved.Consume(line, 50, replacement);
            VERIFY_IS_FALSE(result.progress.has_value());
            VERIFY_IS_FALSE(result.suppressInput);
        }
    }

    void WinTermVisualProgressTests::RecognitionTerminalFailuresClearProviderContext()
    {
        uint64_t timestamp{};

        RecognitionEngine docker;
        for (size_t i = 0; i < RecognitionEngine::DockerLayerCapacity; ++i)
        {
            std::wstring line{ L"layer" };
            line.append(std::to_wstring(i));
            line.append(L": Downloading 1B/2B\n");
            const auto result = docker.Consume(line, timestamp);
            timestamp += RecognitionEngine::PublicationIntervalMilliseconds;
            VERIFY_IS_FALSE(result.overflow);
        }
        const auto dockerError = docker.Consume(L"Error response from daemon: denied\n", timestamp);
        timestamp += RecognitionEngine::PublicationIntervalMilliseconds;
        VERIFY_IS_TRUE(dockerError.progress.has_value());
        if (dockerError.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Error), static_cast<int>(dockerError.progress->status));
        }
        const auto freshLayer = docker.Consume(L"fresh: Downloading 1B/2B\n", timestamp);
        VERIFY_IS_TRUE(freshLayer.healthy);
        VERIFY_IS_FALSE(freshLayer.overflow);

        RecognitionEngine buildKit;
        timestamp = 0;
        for (size_t i = 1; i <= RecognitionEngine::BuildKitStepCapacity; ++i)
        {
            std::wstring line{ L"#" };
            line.append(std::to_wstring(i));
            line.append(L" RUN build 50%\n");
            const auto result = buildKit.Consume(line, timestamp);
            timestamp += RecognitionEngine::PublicationIntervalMilliseconds;
            VERIFY_IS_FALSE(result.overflow);
        }
        const auto buildKitError = buildKit.Consume(L"#1 ERROR: build failed\n", timestamp);
        timestamp += RecognitionEngine::PublicationIntervalMilliseconds;
        VERIFY_IS_TRUE(buildKitError.progress.has_value());
        if (buildKitError.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Error), static_cast<int>(buildKitError.progress->status));
        }
        const auto freshStep = buildKit.Consume(L"#100 RUN build 50%\n", timestamp);
        VERIFY_IS_TRUE(freshStep.healthy);
        VERIFY_IS_FALSE(freshStep.overflow);

        RecognitionEngine pip;
        pip.Consume(L"Downloading demo.whl 50% 1MB/2MB 1MB/s eta 1s\n", 0);
        const auto pipError = pip.Consume(L"error: subprocess-exited-with-error\n", 50);
        VERIFY_IS_TRUE(pipError.progress.has_value());
        const auto unrelated = pip.Consume(L"Downloading backup 50% 1MB/2MB 1MB/s eta 1s\n", 100);
        VERIFY_IS_TRUE(unrelated.progress.has_value());
        if (unrelated.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(unrelated.progress->provider));
            VERIFY_IS_FALSE(unrelated.progress->suppressible);
        }
    }

    void WinTermVisualProgressTests::RecognitionRecoversAndIsolatesEngines()
    {
        RecognitionEngine malformed;
        std::wstring malformedLine;
        malformedLine.push_back(static_cast<wchar_t>(0xdc00));
        malformedLine.append(L"Receiving objects: 50% (50/100)\n");
        VERIFY_IS_FALSE(malformed.Consume(malformedLine, 0).healthy);
        VERIFY_IS_TRUE(malformed.TryReset());
        const auto afterTryReset = malformed.Consume(L"Receiving objects: 50% (50/100)\n", 50);
        VERIFY_IS_TRUE(afterTryReset.healthy);
        VERIFY_IS_TRUE(afterTryReset.progress.has_value());

        malformed.Reset();
        const auto afterReset = malformed.Consume(L"Downloading demo.whl 50% 512kB/1.0MB 1.0MB/s eta 00:01\n", 100);
        VERIFY_IS_TRUE(afterReset.healthy);
        VERIFY_IS_TRUE(afterReset.progress.has_value());
        if (afterReset.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Pip), static_cast<int>(afterReset.progress->provider));
        }

        RecognitionEngine curl;
        curl.Consume(L"% Total    % Received\r\n", 0);
        const auto curlSuccess = curl.Consume(L"100  1024  100  1024  0\n", 50);
        VERIFY_IS_TRUE(curlSuccess.progress.has_value());
        if (curlSuccess.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Success), static_cast<int>(curlSuccess.progress->status));
        }
        const auto afterCurl = curl.Consume(L"42% (42/100) ETA 00:01\n", 100);
        VERIFY_IS_TRUE(afterCurl.progress.has_value());
        if (afterCurl.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(afterCurl.progress->provider));
        }

        RecognitionEngine firstPane;
        RecognitionEngine secondPane;
        const auto firstProgress = firstPane.Consume(L"Receiving objects: 25% (25/100)\n", 0);
        const auto secondProgress = secondPane.Consume(L"Downloading demo.whl 75% 768kB/1.0MB 1.0MB/s eta 00:01\n", 0);
        VERIFY_IS_TRUE(firstProgress.progress.has_value());
        VERIFY_IS_TRUE(secondProgress.progress.has_value());
        if (firstProgress.progress && secondProgress.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Git), static_cast<int>(firstProgress.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Pip), static_cast<int>(secondProgress.progress->provider));
            VERIFY_ARE_EQUAL(uint32_t{ 1 }, firstProgress.progress->sequence);
            VERIFY_ARE_EQUAL(uint32_t{ 1 }, secondProgress.progress->sequence);
        }

        firstPane.Reset();
        const auto secondContinues = secondPane.Consume(L"Downloading demo.whl 80% 819kB/1.0MB 1.0MB/s eta 00:01\n", 50);
        VERIFY_IS_TRUE(secondContinues.progress.has_value());
        if (secondContinues.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Pip), static_cast<int>(secondContinues.progress->provider));
            VERIFY_ARE_EQUAL(uint8_t{ 80 }, secondContinues.progress->value);
        }
    }

    void WinTermVisualProgressTests::RecognitionClassifiesAllProvidersOneCodeUnitAtATime()
    {
        struct Fixture
        {
            std::wstring_view line;
            ProgressProvider provider;
            uint8_t value;
        };

        const std::array<Fixture, 12> fixtures{
            Fixture{ L"demo-layer: Downloading 512B/1.0kB\n", ProgressProvider::DockerPull, 50 },
            Fixture{ L"#7 [3/8] RUN build 50%\n", ProgressProvider::DockerBuildKit, 50 },
            Fixture{ L"Downloading demo.whl 50% 512kB/1.0MB 1.0MB/s eta 00:01\n", ProgressProvider::Pip, 50 },
            Fixture{ L"Receiving objects: 50% (50/100)\n", ProgressProvider::Git, 50 },
            Fixture{ L"wget demo 50%[====>     ] 512K 1.0MB/s eta 1s\n", ProgressProvider::Wget, 50 },
            Fixture{ L"npm fetch 50% (5/10)\n", ProgressProvider::Npm, 50 },
            Fixture{ L"pnpm download 50% (5/10)\n", ProgressProvider::Pnpm, 50 },
            Fixture{ L"yarn fetch 50% (5/10)\n", ProgressProvider::Yarn, 50 },
            Fixture{ L"nvm downloading node.js 50%\n", ProgressProvider::Nvm, 50 },
            Fixture{ L"[INFO] Progress (1): 512/1024 kB\n", ProgressProvider::Maven, 50 },
            Fixture{ L"75% EXECUTING\n", ProgressProvider::Gradle, 75 },
            Fixture{ L"42% (42/100) 1.0 MB/s ETA 00:05\n", ProgressProvider::Generic, 42 },
        };

        const auto feedOneCodeUnitAtATime = [](RecognitionEngine& engine,
                                               const std::wstring_view text,
                                               uint64_t& timestamp) {
            std::optional<ProviderProgress> latest;
            for (size_t i = 0; i < text.size(); ++i)
            {
                const auto result = engine.Consume(text.substr(i, 1), timestamp);
                timestamp += RecognitionEngine::PublicationIntervalMilliseconds;
                VERIFY_IS_TRUE(result.accepted);
                VERIFY_IS_TRUE(result.healthy);
                VERIFY_IS_FALSE(result.suppressInput);
                if (result.progress)
                {
                    latest = result.progress;
                }
            }
            return latest;
        };

        for (const auto& fixture : fixtures)
        {
            RecognitionEngine engine;
            uint64_t timestamp{};
            const auto progress = feedOneCodeUnitAtATime(engine, fixture.line, timestamp);
            VERIFY_IS_TRUE(progress.has_value());
            if (progress)
            {
                VERIFY_ARE_EQUAL(static_cast<int>(fixture.provider), static_cast<int>(progress->provider));
                VERIFY_ARE_EQUAL(fixture.value, progress->value);
            }
        }

        RecognitionEngine curl;
        uint64_t curlTimestamp{};
        const auto header = feedOneCodeUnitAtATime(curl, L"% Total    % Received\r\n", curlTimestamp);
        VERIFY_IS_TRUE(header.has_value());
        const auto meter = feedOneCodeUnitAtATime(curl, L"50  1024  50  512  0\n", curlTimestamp);
        VERIFY_IS_TRUE(meter.has_value());
        if (meter)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Curl), static_cast<int>(meter->provider));
            VERIFY_ARE_EQUAL(uint8_t{ 50 }, meter->value);
        }
    }

    void WinTermVisualProgressTests::RecognitionRejectsMalformedNumericAndInterruptedOutput()
    {
        for (const auto line : {
                 L"101% (101/100) ETA 00:01\n",
                 L"-1% ETA 00:01\n",
                 L"50.5% ETA 00:01\n",
                 L"1/0 ETA 00:01\n",
                 L"200/100 ETA 00:01\n",
                 L"184467440737095516160/2 ETA 00:01\n",
                 L"512XB/1MB ETA 00:01\n" })
        {
            RecognitionEngine engine;
            const auto result = engine.Consume(line, 0);
            VERIFY_IS_FALSE(result.progress.has_value());
            VERIFY_IS_FALSE(result.suppressInput);
        }

        RecognitionOptions replacement;
        replacement.replacementEnabled = true;
        replacement.rendererEnabled = true;

        RecognitionEngine malformedGit;
        malformedGit.Consume(L"\r", 0);
        const auto git = malformedGit.Consume(L"Receiving objects: 101% (101/100)\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(git.progress.has_value());
        if (git.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Git), static_cast<int>(git.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(git.progress->mode));
            VERIFY_IS_FALSE(git.progress->suppressible);
        }
        VERIFY_IS_FALSE(git.suppressInput);

        RecognitionEngine malformedDocker;
        const auto docker = malformedDocker.Consume(L"demo-layer: Downloading 512XB/1MB\n", 0);
        VERIFY_IS_TRUE(docker.progress.has_value());
        if (docker.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::DockerPull), static_cast<int>(docker.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(docker.progress->mode));
        }
        VERIFY_IS_FALSE(docker.suppressInput);

        RecognitionEngine malformedCurl;
        malformedCurl.Consume(L"% Total    % Received\r\n", 0);
        VERIFY_IS_FALSE(malformedCurl.Consume(L"50  1024\n", 50).progress.has_value());

        RecognitionEngine interrupted;
        VERIFY_IS_FALSE(interrupted.Consume(L"ordinary output 4", 0).progress.has_value());
        const auto ordinaryTail = interrupted.Consume(L"2% complete\n", 50);
        VERIFY_IS_FALSE(ordinaryTail.progress.has_value());
        VERIFY_IS_FALSE(ordinaryTail.suppressInput);

        RecognitionEngine explanatoryText;
        const auto explanation = explanatoryText.Consume(L"documentation: Receiving objects: 50% is an example\n", 0);
        VERIFY_IS_FALSE(explanation.progress.has_value());
        VERIFY_IS_FALSE(explanation.suppressInput);

        RecognitionEngine interruptedNumber;
        VERIFY_IS_FALSE(interruptedNumber.Consume(L"42", 0).progress.has_value());
        VERIFY_IS_FALSE(interruptedNumber.Consume(L" files copied\n", 50).progress.has_value());
    }

    void WinTermVisualProgressTests::RecognitionPreservesHighConfidenceOwnershipAndClearsGeneric()
    {
        RecognitionEngine owned;
        const auto initial = owned.Consume(L"Receiving objects: 40% (40/100)\n", 0);
        VERIFY_IS_TRUE(initial.progress.has_value());
        if (initial.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Git), static_cast<int>(initial.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProviderConfidence::High), static_cast<int>(initial.progress->confidence));
        }

        const auto genericLooking = owned.Consume(L"42% (42/100) 1.0 MB/s ETA 00:05\n", 50);
        VERIFY_IS_FALSE(genericLooking.progress.has_value());
        VERIFY_IS_FALSE(genericLooking.suppressInput);

        const auto continued = owned.Consume(L"Receiving objects: 60% (60/100)\n", 100);
        VERIFY_IS_TRUE(continued.progress.has_value());
        if (continued.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Git), static_cast<int>(continued.progress->provider));
            VERIFY_ARE_EQUAL(uint8_t{ 60 }, continued.progress->value);
        }

        RecognitionEngine expiredOwnership;
        const auto claimed = expiredOwnership.Consume(L"Receiving objects: 40% (40/100)\n", 0);
        VERIFY_IS_TRUE(claimed.progress.has_value());
        const auto ordinary = expiredOwnership.Consume(L"ordinary command output\n", 50);
        VERIFY_IS_FALSE(ordinary.progress.has_value());
        const auto laterGeneric = expiredOwnership.Consume(L"42% (42/100) 1.0 MB/s ETA 00:05\n", 100);
        VERIFY_IS_TRUE(laterGeneric.progress.has_value());
        if (laterGeneric.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(laterGeneric.progress->provider));
            VERIFY_IS_FALSE(laterGeneric.progress->suppressible);
        }
        VERIFY_IS_FALSE(laterGeneric.suppressInput);

        RecognitionOptions replacement;
        replacement.replacementEnabled = true;
        replacement.rendererEnabled = true;

        RecognitionEngine staleCurl;
        VERIFY_IS_TRUE(staleCurl.Consume(L"% Total    % Received\n", 0, replacement).progress.has_value());
        VERIFY_IS_FALSE(staleCurl.Consume(L"ordinary command output\n", 50, replacement).progress.has_value());
        const auto laterCurlShape = staleCurl.Consume(L"50  1024  50  512  0\r\x1b[2K", 100, replacement);
        VERIFY_IS_FALSE(laterCurlShape.progress.has_value());
        VERIFY_IS_FALSE(laterCurlShape.suppressInput);

        RecognitionEngine staleWget;
        VERIFY_IS_TRUE(staleWget.Consume(L"Saving to: 'demo.bin'\n", 0, replacement).progress.has_value());
        VERIFY_IS_FALSE(staleWget.Consume(L"ordinary command output\n", 50, replacement).progress.has_value());
        const auto laterWgetShape = staleWget.Consume(
            L"demo.bin 50%[====>     ] 512K 1.0MB/s eta 1s\r\x1b[2K",
            100,
            replacement);
        // The stale shape is not reclaimed by wget. As the second consecutive
        // record without a matching provider, it structurally clears the
        // dangling wget bar instead of leaving it running.
        VERIFY_IS_TRUE(laterWgetShape.progress.has_value());
        if (laterWgetShape.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::None), static_cast<int>(laterWgetShape.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Hidden), static_cast<int>(laterWgetShape.progress->mode));
            VERIFY_IS_FALSE(laterWgetShape.progress->visible);
        }
        VERIFY_IS_FALSE(laterWgetShape.suppressInput);

        RecognitionEngine generic;
        const auto visible = generic.Consume(L"42% (42/100) 1.0 MB/s ETA 00:05\n", 0);
        VERIFY_IS_TRUE(visible.progress.has_value());
        if (visible.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(visible.progress->provider));
            VERIFY_IS_TRUE(visible.progress->visible);
        }

        const auto cleared = generic.Consume(L"ordinary command output\n", 1);
        VERIFY_IS_TRUE(cleared.progress.has_value());
        if (cleared.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::None), static_cast<int>(cleared.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Hidden), static_cast<int>(cleared.progress->mode));
            VERIFY_IS_FALSE(cleared.progress->visible);
        }
        VERIFY_IS_FALSE(cleared.suppressInput);
    }

    void WinTermVisualProgressTests::RecognitionIgnoresProductMentionsInListings()
    {
        // A directory listing is ordinary output. An entry that mentions a
        // build tool by name, or a slashed date column, must not start a bar.
        RecognitionEngine listing;
        VERIFY_IS_FALSE(listing.Consume(L"d-----        2025/10/13     01:28                .gradle\n", 0).progress.has_value());
        VERIFY_IS_FALSE(listing.Consume(L"d-----        2025/10/13     01:28                Downloads\n", 50).progress.has_value());
        VERIFY_IS_FALSE(listing.Consume(L"-a----        2025/10/13     01:28            185 notes.ini\n", 100).progress.has_value());
        VERIFY_IS_FALSE(listing.Consume(L"-a----        01/10/2025     01:28             46 setup.log\n", 150).progress.has_value());
        VERIFY_IS_FALSE(listing.Consume(L"PS C:\\demo> cd \\\n", 200).progress.has_value());

        // An established claim must carry per-record evidence to rematch; a
        // later arbitrary record must not refresh the bar.
        RecognitionEngine claimed;
        const auto task = claimed.Consume(L"> Task :app:compileJava\n", 0);
        VERIFY_IS_TRUE(task.progress.has_value());
        if (task.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Gradle), static_cast<int>(task.progress->provider));
            VERIFY_IS_TRUE(task.progress->visible);
        }
        VERIFY_IS_FALSE(claimed.Consume(L"PS C:\\demo> dir\n", 50).progress.has_value());

        // The status meter keeps matching through its own real value.
        const auto meter = claimed.Consume(L"<=========----> 75% EXECUTING [16s]\n", 100);
        VERIFY_IS_TRUE(meter.progress.has_value());
        if (meter.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Gradle), static_cast<int>(meter.progress->provider));
            VERIFY_ARE_EQUAL(uint8_t{ 75 }, meter.progress->value);
        }
    }

    void WinTermVisualProgressTests::RecognitionClearsStaleRunningProviderAfterOrdinaryRecords()
    {
        // A still-running provider bar tolerates one ordinary record, and the
        // second consecutive ordinary record publishes a structural clear.
        RecognitionEngine stale;
        const auto claimed = stale.Consume(L"Downloading https://services.gradle.org/distributions/gradle-8.5-bin.zip\n", 0);
        VERIFY_IS_TRUE(claimed.progress.has_value());
        if (claimed.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Gradle), static_cast<int>(claimed.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(claimed.progress->mode));
            VERIFY_IS_TRUE(claimed.progress->visible);
        }
        VERIFY_IS_FALSE(stale.Consume(L"ordinary command output\n", 50).progress.has_value());
        const auto cleared = stale.Consume(L"PS C:\\demo> dir\n", 100);
        VERIFY_IS_TRUE(cleared.progress.has_value());
        if (cleared.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::None), static_cast<int>(cleared.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Hidden), static_cast<int>(cleared.progress->mode));
            VERIFY_IS_FALSE(cleared.progress->visible);
        }

        // Success and Error are final results and persist across ordinary
        // output until a later publication replaces them.
        RecognitionEngine finished;
        const auto success = finished.Consume(L"npm completed\n", 0);
        VERIFY_IS_TRUE(success.progress.has_value());
        if (success.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Success), static_cast<int>(success.progress->status));
        }
        VERIFY_IS_FALSE(finished.Consume(L"ordinary command output\n", 50).progress.has_value());
        VERIFY_IS_FALSE(finished.Consume(L"more ordinary command output\n", 100).progress.has_value());
    }

    void WinTermVisualProgressTests::RecognitionBootstrapsRichPipAndMavenResolver()
    {
        RecognitionEngine pip;
        const auto announcement = pip.Consume(
            L"Downloading demo_package-1.0-py3-none-any.whl (2.0 MB)\n",
            0);
        VERIFY_IS_TRUE(announcement.progress.has_value());
        if (announcement.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Pip), static_cast<int>(announcement.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(announcement.progress->mode));
        }

        const auto rich = pip.Consume(
            L"\x2501\x2501\x2501\x2501 1.0/2.0 MB 4.0 MB/s eta 0:00:01\r\x1b[2K",
            50);
        VERIFY_IS_TRUE(rich.progress.has_value());
        if (rich.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Pip), static_cast<int>(rich.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Determinate), static_cast<int>(rich.progress->mode));
            VERIFY_ARE_EQUAL(uint8_t{ 50 }, rich.progress->value);
            VERIFY_ARE_EQUAL(static_cast<int>(ProviderConfidence::High), static_cast<int>(rich.progress->confidence));
        }
        VERIFY_IS_FALSE(rich.suppressInput);

        const auto summary = pip.Consume(L"Successfully installed demo-package\n", 100);
        VERIFY_IS_FALSE(summary.progress.has_value());
        VERIFY_IS_FALSE(summary.suppressInput);

        RecognitionOptions replacement;
        replacement.replacementEnabled = true;
        replacement.rendererEnabled = true;

        RecognitionEngine unrelatedArchive;
        const auto unrelatedAnnouncement = unrelatedArchive.Consume(L"Downloading backup.zip (2.0 MB)\n", 0, replacement);
        VERIFY_IS_FALSE(unrelatedAnnouncement.progress.has_value());
        const auto unrelatedMeter = unrelatedArchive.Consume(
            L"\x2501\x2501\x2501\x2501 1.0/2.0 MB 4.0 MB/s eta 0:00:01\r\x1b[2K",
            50,
            replacement);
        VERIFY_IS_TRUE(unrelatedMeter.progress.has_value());
        if (unrelatedMeter.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(unrelatedMeter.progress->provider));
        }
        VERIFY_IS_FALSE(unrelatedMeter.suppressInput);

        RecognitionEngine archiveSubstring;
        const auto substring = archiveSubstring.Consume(
            L"Downloading backup.zipper 50% 1MB/2MB 1MB/s eta 1s\r\x1b[2K",
            0,
            replacement);
        VERIFY_IS_TRUE(substring.progress.has_value());
        if (substring.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(substring.progress->provider));
        }
        VERIFY_IS_FALSE(substring.suppressInput);

        RecognitionEngine trustedSourceArchive;
        const auto collecting = trustedSourceArchive.Consume(L"Collecting demo-package\n", 0, replacement);
        VERIFY_IS_TRUE(collecting.progress.has_value());
        const auto sourceAnnouncement = trustedSourceArchive.Consume(
            L"Downloading demo-package.tar.gz (2.0 MB)\n",
            50,
            replacement);
        // This announcement is structurally identical to the preceding
        // indeterminate pip update, so publication may be coalesced.
        if (sourceAnnouncement.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Pip), static_cast<int>(sourceAnnouncement.progress->provider));
        }
        VERIFY_IS_FALSE(sourceAnnouncement.suppressInput);
        const auto sourceMeter = trustedSourceArchive.Consume(
            L"\x2501\x2501\x2501\x2501 1.0/2.0 MB 4.0 MB/s eta 0:00:01\r\x1b[2K",
            100,
            replacement);
        VERIFY_IS_TRUE(sourceMeter.progress.has_value());
        if (sourceMeter.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Pip), static_cast<int>(sourceMeter.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProviderConfidence::High), static_cast<int>(sourceMeter.progress->confidence));
        }

        RecognitionEngine maven;
        const auto download = maven.Consume(
            L"Downloading from central: https://repo.example.invalid/artifact.jar\n",
            0);
        VERIFY_IS_TRUE(download.progress.has_value());
        if (download.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Maven), static_cast<int>(download.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(download.progress->mode));
        }

        const auto resolver = maven.Consume(L"Progress (1): 512/1024 kB\r\x1b[2K", 50);
        VERIFY_IS_TRUE(resolver.progress.has_value());
        if (resolver.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Maven), static_cast<int>(resolver.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Determinate), static_cast<int>(resolver.progress->mode));
            VERIFY_ARE_EQUAL(uint8_t{ 50 }, resolver.progress->value);
            VERIFY_IS_FALSE(resolver.progress->suppressible);
        }
        VERIFY_IS_FALSE(resolver.suppressInput);

        RecognitionEngine taggedMaven;
        const auto taggedDownload = taggedMaven.Consume(
            L"[INFO] Downloading from central: https://repo.example.invalid/artifact.jar\n",
            0);
        VERIFY_IS_TRUE(taggedDownload.progress.has_value());
        if (taggedDownload.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Maven), static_cast<int>(taggedDownload.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(taggedDownload.progress->mode));
        }

        RecognitionEngine mavenStage;
        const auto tests = mavenStage.Consume(L"[INFO] Tests run: 5/10\n", 0);
        VERIFY_IS_TRUE(tests.progress.has_value());
        if (tests.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Maven), static_cast<int>(tests.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(tests.progress->mode));
        }
    }

    void WinTermVisualProgressTests::RecognitionHandlesGenericIndeterminateShapes()
    {
        RecognitionOptions replacement;
        replacement.replacementEnabled = true;
        replacement.rendererEnabled = true;

        RecognitionEngine spinner;
        spinner.Consume(L"\r", 0);
        const auto spinnerResult = spinner.Consume(L"|\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(spinnerResult.progress.has_value());
        if (spinnerResult.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(spinnerResult.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(spinnerResult.progress->mode));
            VERIFY_IS_FALSE(spinnerResult.progress->suppressible);
        }
        VERIFY_IS_FALSE(spinnerResult.suppressInput);

        RecognitionEngine speedAndEta;
        const auto transfer = speedAndEta.Consume(L"4.0 MB/s ETA 00:03\r\x1b[2K", 0, replacement);
        VERIFY_IS_TRUE(transfer.progress.has_value());
        if (transfer.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(transfer.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(transfer.progress->mode));
            VERIFY_IS_FALSE(transfer.progress->suppressible);
        }
        VERIFY_IS_FALSE(transfer.suppressInput);

        RecognitionEngine repeated;
        const auto first = repeated.Consume(L"processed 1\r\x1b[2K", 0, replacement);
        VERIFY_IS_FALSE(first.progress.has_value());
        VERIFY_IS_FALSE(first.suppressInput);
        const auto second = repeated.Consume(L"processed 2\r\x1b[2K", 50, replacement);
        VERIFY_IS_TRUE(second.progress.has_value());
        if (second.progress)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::Generic), static_cast<int>(second.progress->provider));
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(second.progress->mode));
            VERIFY_IS_FALSE(second.progress->suppressible);
        }
        VERIFY_IS_FALSE(second.suppressInput);

        const auto clear = repeated.Consume(L"finished ordinary work\n", 51, replacement);
        VERIFY_IS_TRUE(clear.progress.has_value());
        if (clear.progress)
        {
            VERIFY_IS_FALSE(clear.progress->visible);
            VERIFY_ARE_EQUAL(static_cast<int>(ProgressProvider::None), static_cast<int>(clear.progress->provider));
        }
        VERIFY_IS_FALSE(clear.suppressInput);
    }

    void WinTermVisualProgressTests::RecognitionHandlesArbitraryProviderSplitsAndReset()
    {
        struct Fixture
        {
            std::wstring_view stream;
            ProgressProvider provider;
        };

        const std::array<Fixture, 13> fixtures{
            Fixture{ L"demo-layer: Downloading 512B/1.0kB\n", ProgressProvider::DockerPull },
            Fixture{ L"#7 [3/8] RUN build 50%\n", ProgressProvider::DockerBuildKit },
            Fixture{ L"Downloading demo_package-1.0-py3-none-any.whl (2.0 MB)\n\x2501\x2501 1.0/2.0 MB 4.0 MB/s eta 0:00:01\n", ProgressProvider::Pip },
            Fixture{ L"Receiving objects: 50% (50/100)\n", ProgressProvider::Git },
            Fixture{ L"% Total    % Received\r\n50  1024  50  512  0\n", ProgressProvider::Curl },
            Fixture{ L"Saving to: 'demo.bin'\ndemo.bin 50%[====>     ] 512K 1.0MB/s eta 1s\n", ProgressProvider::Wget },
            Fixture{ L"npm fetch 50% (5/10)\n", ProgressProvider::Npm },
            Fixture{ L"pnpm download 50% (5/10)\n", ProgressProvider::Pnpm },
            Fixture{ L"yarn fetch 50% (5/10)\n", ProgressProvider::Yarn },
            Fixture{ L"nvm downloading node.js 50%\n", ProgressProvider::Nvm },
            Fixture{ L"Downloading from central: https://repo.example.invalid/artifact.jar\nProgress (1): 512/1024 kB\n", ProgressProvider::Maven },
            Fixture{ L"75% EXECUTING\n", ProgressProvider::Gradle },
            Fixture{ L"42% (42/100) 1.0 MB/s ETA 00:05\n", ProgressProvider::Generic },
        };

        const auto remember = [](std::optional<ProviderProgress>& latest, const RecognitionResult& result) {
            if (result.progress)
            {
                latest = result.progress;
            }
        };

        for (const auto& fixture : fixtures)
        {
            for (size_t split = 1; split < fixture.stream.size(); ++split)
            {
                RecognitionEngine engine;
                std::optional<ProviderProgress> latest;
                remember(latest, engine.Consume(fixture.stream.substr(0, split), 0));
                remember(latest, engine.Consume(fixture.stream.substr(split), 50));
                VERIFY_IS_TRUE(latest.has_value());
                if (latest)
                {
                    VERIFY_ARE_EQUAL(static_cast<int>(fixture.provider), static_cast<int>(latest->provider));
                }
            }

            RecognitionEngine reset;
            reset.Consume(fixture.stream.substr(0, fixture.stream.size() / 2), 0);
            VERIFY_IS_TRUE(reset.TryReset());
            const auto afterReset = reset.Consume(fixture.stream, 50);
            VERIFY_IS_TRUE(afterReset.progress.has_value());
            if (afterReset.progress)
            {
                VERIFY_ARE_EQUAL(static_cast<int>(fixture.provider), static_cast<int>(afterReset.progress->provider));
            }
            VERIFY_IS_FALSE(afterReset.suppressInput);
        }
    }

    void WinTermVisualProgressTests::RecognitionAppliesSuppressionSafetyMatrix()
    {
        struct Fixture
        {
            std::wstring_view prelude;
            std::wstring_view transientRecord;
            std::wstring_view newlineRecord;
            ProgressProvider provider;
            bool safelyReplaceable;
        };

        const std::array<Fixture, 13> fixtures{
            Fixture{ {}, L"demo-layer: Downloading 512B/1.0kB\r\x1b[2K", L"demo-layer: Downloading 512B/1.0kB\n", ProgressProvider::DockerPull, false },
            Fixture{ {}, L"#7 [3/8] RUN build 50%\r\x1b[2K", L"#7 [3/8] RUN build 50%\n", ProgressProvider::DockerBuildKit, false },
            Fixture{ L"Downloading demo_package-1.0-py3-none-any.whl (2.0 MB)\n", L"\x2501\x2501 1.0/2.0 MB 4.0 MB/s eta 0:00:01\r\x1b[2K", L"\x2501\x2501 1.0/2.0 MB 4.0 MB/s eta 0:00:01\n", ProgressProvider::Pip, true },
            Fixture{ {}, L"Receiving objects: 50% (50/100)\r\x1b[2K", L"Receiving objects: 50% (50/100)\n", ProgressProvider::Git, true },
            Fixture{ L"% Total    % Received\r\n", L"50  1024  50  512  0\r\x1b[2K", L"50  1024  50  512  0\n", ProgressProvider::Curl, true },
            Fixture{ L"Saving to: 'demo.bin'\n", L"demo.bin 50%[====>     ] 512K 1.0MB/s eta 1s\r\x1b[2K", L"demo.bin 50%[====>     ] 512K 1.0MB/s eta 1s\n", ProgressProvider::Wget, true },
            Fixture{ {}, L"npm fetch 50% (5/10)\r\x1b[2K", L"npm fetch 50% (5/10)\n", ProgressProvider::Npm, false },
            Fixture{ {}, L"pnpm download 50% (5/10)\r\x1b[2K", L"pnpm download 50% (5/10)\n", ProgressProvider::Pnpm, false },
            Fixture{ {}, L"yarn fetch 50% (5/10)\r\x1b[2K", L"yarn fetch 50% (5/10)\n", ProgressProvider::Yarn, false },
            Fixture{ {}, L"nvm downloading node.js 50%\r\x1b[2K", L"nvm downloading node.js 50%\n", ProgressProvider::Nvm, false },
            Fixture{ L"Downloading from central: https://repo.example.invalid/artifact.jar\n", L"Progress (1): 512/1024 kB\r\x1b[2K", L"Progress (1): 512/1024 kB\n", ProgressProvider::Maven, false },
            Fixture{ {}, L"75% EXECUTING\r\x1b[2K", L"75% EXECUTING\n", ProgressProvider::Gradle, false },
            Fixture{ {}, L"42% (42/100) 1.0 MB/s ETA 00:05\r\x1b[2K", L"42% (42/100) 1.0 MB/s ETA 00:05\n", ProgressProvider::Generic, false },
        };

        RecognitionOptions replacement;
        replacement.replacementEnabled = true;
        replacement.rendererEnabled = true;

        const auto prepare = [](RecognitionEngine& engine,
                                const std::wstring_view prelude,
                                uint64_t& timestamp) {
            if (!prelude.empty())
            {
                engine.Consume(prelude, timestamp);
                timestamp += RecognitionEngine::PublicationIntervalMilliseconds;
            }
            engine.Consume(L"\r", timestamp);
            timestamp += RecognitionEngine::PublicationIntervalMilliseconds;
        };

        for (const auto& fixture : fixtures)
        {
            RecognitionEngine whole;
            uint64_t timestamp{};
            prepare(whole, fixture.prelude, timestamp);
            const auto wholeResult = whole.Consume(fixture.transientRecord, timestamp, replacement);
            VERIFY_IS_TRUE(wholeResult.progress.has_value());
            if (wholeResult.progress)
            {
                VERIFY_ARE_EQUAL(static_cast<int>(fixture.provider), static_cast<int>(wholeResult.progress->provider));
                VERIFY_ARE_EQUAL(fixture.safelyReplaceable, wholeResult.progress->suppressible);
            }
            VERIFY_ARE_EQUAL(fixture.safelyReplaceable, wholeResult.suppressInput);

            RecognitionEngine newline;
            timestamp = 0;
            prepare(newline, fixture.prelude, timestamp);
            const auto newlineResult = newline.Consume(fixture.newlineRecord, timestamp, replacement);
            VERIFY_IS_TRUE(newlineResult.progress.has_value());
            if (newlineResult.progress)
            {
                VERIFY_ARE_EQUAL(static_cast<int>(fixture.provider), static_cast<int>(newlineResult.progress->provider));
                VERIFY_IS_FALSE(newlineResult.progress->suppressible);
            }
            VERIFY_IS_FALSE(newlineResult.suppressInput);

            RecognitionEngine fragmented;
            timestamp = 0;
            prepare(fragmented, fixture.prelude, timestamp);
            const auto split = fixture.transientRecord.size() / 2;
            std::optional<ProviderProgress> fragmentedProgress;
            const auto prefix = fragmented.Consume(fixture.transientRecord.substr(0, split), timestamp, replacement);
            if (prefix.progress)
            {
                fragmentedProgress = prefix.progress;
            }
            const auto suffix = fragmented.Consume(
                fixture.transientRecord.substr(split),
                timestamp + RecognitionEngine::PublicationIntervalMilliseconds,
                replacement);
            if (suffix.progress)
            {
                fragmentedProgress = suffix.progress;
            }
            VERIFY_IS_TRUE(fragmentedProgress.has_value());
            if (fragmentedProgress)
            {
                VERIFY_ARE_EQUAL(static_cast<int>(fixture.provider), static_cast<int>(fragmentedProgress->provider));
            }
            VERIFY_IS_FALSE(prefix.suppressInput);
            VERIFY_IS_FALSE(suffix.suppressInput);

            auto rendererUnavailable = replacement;
            rendererUnavailable.rendererEnabled = false;
            auto alternateScreen = replacement;
            alternateScreen.normalScreen = false;
            auto parserUnavailable = replacement;
            parserUnavailable.parserHealthy = false;
            for (const auto gated : { rendererUnavailable, alternateScreen, parserUnavailable })
            {
                RecognitionEngine gatedEngine;
                timestamp = 0;
                prepare(gatedEngine, fixture.prelude, timestamp);
                const auto gatedResult = gatedEngine.Consume(fixture.transientRecord, timestamp, gated);
                VERIFY_IS_TRUE(gatedResult.progress.has_value());
                if (gatedResult.progress)
                {
                    VERIFY_ARE_EQUAL(static_cast<int>(fixture.provider), static_cast<int>(gatedResult.progress->provider));
                }
                VERIFY_IS_FALSE(gatedResult.suppressInput);
            }
        }
    }

    void WinTermVisualProgressTests::RendererPlansRealValuesRegressionAndIndeterminateMode()
    {
        RenderEnvironment environment;
        environment.hostLoaded = true;
        environment.tabVisible = true;
        environment.paneActive = true;

        VisualProgressRenderState renderer;
        for (const auto value : { uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 50 }, uint8_t{ 99 }, uint8_t{ 100 } })
        {
            const auto plan = renderer.Apply(
                { ProgressMode::Determinate, ProgressStatus::Running, value, true, ProgressSource::Taskbar, value },
                environment,
                RenderTimestamp{ static_cast<int64_t>(value) * 1000 });
            VERIFY_ARE_EQUAL(static_cast<float>(value) / 100.0f, plan.targetProgress);
            VERIFY_ARE_EQUAL(value > 0, plan.headVisible);
        }

        const auto regression = renderer.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 1, true, ProgressSource::Provider, 200 },
            environment,
            RenderTimestamp{ 101000 });
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTransitionKind::PhaseRegression), static_cast<int>(regression.kind));
        VERIFY_IS_TRUE(regression.phaseReset);
        VERIFY_ARE_EQUAL(0.01f, regression.targetProgress);

        const auto indeterminate = renderer.Apply(
            { ProgressMode::Indeterminate, ProgressStatus::Running, 0, true, ProgressSource::Provider, 201 },
            environment,
            RenderTimestamp{ 102000 });
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTransitionKind::Indeterminate), static_cast<int>(indeterminate.kind));
        VERIFY_IS_TRUE(indeterminate.indeterminateMoving);
        VERIFY_IS_TRUE(indeterminate.headVisible);
    }

    void WinTermVisualProgressTests::RendererHiddenIngressAndOwnershipBoundaries()
    {
        RenderEnvironment hiddenEnvironment;
        hiddenEnvironment.hostLoaded = false;
        hiddenEnvironment.tabVisible = false;
        hiddenEnvironment.paneActive = false;

        VisualProgressRenderState hiddenIngress;
        const auto unloaded = hiddenIngress.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 73, true, ProgressSource::Provider, 1 },
            hiddenEnvironment,
            RenderTimestamp{ 0 });
        VERIFY_IS_FALSE(unloaded.visible);
        VERIFY_IS_FALSE(hiddenIngress.Presenting());
        VERIFY_ARE_EQUAL(0.73f, unloaded.targetProgress);

        hiddenEnvironment.hostLoaded = true;
        const auto loadedInHiddenTab = hiddenIngress.RefreshEnvironment(hiddenEnvironment, RenderTimestamp{ 500 });
        VERIFY_IS_FALSE(loadedInHiddenTab.visible);
        VERIFY_IS_FALSE(loadedInHiddenTab.rainbowMoving);
        VERIFY_IS_FALSE(RequiresSparkWork(loadedInHiddenTab, 0));

        hiddenEnvironment.tabVisible = true;
        const auto visibleInactive = hiddenIngress.RefreshEnvironment(hiddenEnvironment, RenderTimestamp{ 750 });
        VERIFY_IS_TRUE(visibleInactive.visible);
        VERIFY_IS_FALSE(visibleInactive.rainbowMoving);
        VERIFY_IS_FALSE(visibleInactive.sparksEligible);
        VERIFY_ARE_EQUAL(0.73f, visibleInactive.targetProgress);

        hiddenEnvironment.paneActive = true;
        const auto visibleActive = hiddenIngress.RefreshEnvironment(hiddenEnvironment, RenderTimestamp{ 1000 });
        VERIFY_IS_TRUE(visibleActive.visible);
        VERIFY_IS_TRUE(visibleActive.rainbowMoving);
        VERIFY_IS_TRUE(visibleActive.sparksEligible);
        VERIFY_IS_TRUE(RequiresSparkWork(visibleActive, 0));
        VERIFY_ARE_EQUAL(0.73f, hiddenIngress.CurrentProgress(RenderTimestamp{ 1000 }));

        hiddenEnvironment.paneActive = false;
        const auto visibleBackground = hiddenIngress.RefreshEnvironment(hiddenEnvironment, RenderTimestamp{ 1500 });
        VERIFY_IS_TRUE(visibleBackground.visible);
        VERIFY_IS_FALSE(visibleBackground.rainbowMoving);
        VERIFY_IS_FALSE(visibleBackground.sparksEligible);
        VERIFY_IS_FALSE(RequiresSparkWork(visibleBackground, 0));

        hiddenEnvironment.paneActive = true;

        const auto explicitlyHidden = hiddenIngress.Apply(
            { ProgressMode::Hidden, ProgressStatus::Cancelled, 0, false, ProgressSource::None, 2 },
            hiddenEnvironment,
            RenderTimestamp{ 2000 });
        VERIFY_IS_FALSE(explicitlyHidden.visible);
        VERIFY_ARE_EQUAL(0.0f, explicitlyHidden.targetProgress);
        VERIFY_ARE_EQUAL(0.0f, hiddenIngress.CurrentProgress(RenderTimestamp{ 2000 }));
        VERIFY_IS_TRUE(explicitlyHidden.releaseAfterTransition);
        VERIFY_IS_FALSE(RequiresSparkWork(explicitlyHidden, 0));

        for (const auto status : { ProgressStatus::Error, ProgressStatus::Waiting })
        {
            VisualProgressRenderState ownershipBoundary;
            ownershipBoundary.Apply(
                { ProgressMode::Determinate, ProgressStatus::Running, 60, true, ProgressSource::Provider, 1 },
                hiddenEnvironment,
                RenderTimestamp{ 0 });
            ownershipBoundary.Apply(
                { ProgressMode::Hidden, ProgressStatus::Cancelled, 0, false, ProgressSource::None, 2 },
                hiddenEnvironment,
                RenderTimestamp{ 1000 });

            const auto zeroStatus = ownershipBoundary.Apply(
                { ProgressMode::Determinate, status, 0, true, ProgressSource::Provider, 3 },
                hiddenEnvironment,
                RenderTimestamp{ 2000 });
            VERIFY_ARE_EQUAL(0.0f, zeroStatus.targetProgress);

            const auto nextCommand = ownershipBoundary.Apply(
                { ProgressMode::Determinate, ProgressStatus::Running, 20, true, ProgressSource::Provider, 4 },
                hiddenEnvironment,
                RenderTimestamp{ 3000 });
            VERIFY_ARE_NOT_EQUAL(
                static_cast<int>(RenderTransitionKind::PhaseRegression),
                static_cast<int>(nextCommand.kind));
            VERIFY_IS_FALSE(nextCommand.phaseReset);
            VERIFY_ARE_EQUAL(0.2f, nextCommand.targetProgress);
        }
    }

    void WinTermVisualProgressTests::RendererPlansStatusAndAccessibilityFallbacks()
    {
        RenderEnvironment environment;
        environment.hostLoaded = true;
        environment.tabVisible = true;
        environment.paneActive = true;

        VisualProgressRenderState renderer;
        renderer.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 65, true, ProgressSource::Taskbar, 1 },
            environment,
            RenderTimestamp{ 0 });

        const auto waiting = renderer.Apply(
            { ProgressMode::Determinate, ProgressStatus::Waiting, 0, true, ProgressSource::Taskbar, 2 },
            environment,
            RenderTimestamp{ 1000 });
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTransitionKind::Waiting), static_cast<int>(waiting.kind));
        VERIFY_ARE_EQUAL(0.65f, waiting.targetProgress);
        VERIFY_IS_TRUE(waiting.breathe);
        VERIFY_IS_FALSE(waiting.sparksEligible);

        const auto success = renderer.Apply(
            { ProgressMode::Determinate, ProgressStatus::Success, 100, true, ProgressSource::Taskbar, 3 },
            environment,
            RenderTimestamp{ 2000 });
        VERIFY_IS_TRUE(success.successSweep);
        VERIFY_IS_TRUE(success.finalSparkBurst);
        VERIFY_IS_TRUE(success.fadeOut);
        VERIFY_IS_TRUE(success.releaseAfterTransition);
        VERIFY_IS_TRUE(RequiresSparkWork(success, 0));

        environment.paneActive = false;
        const auto successInactiveRefresh = renderer.RefreshEnvironment(environment, RenderTimestamp{ 2250 });
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTransitionKind::EnvironmentRefresh), static_cast<int>(successInactiveRefresh.kind));
        VERIFY_IS_FALSE(successInactiveRefresh.successSweep);
        VERIFY_IS_FALSE(successInactiveRefresh.finalSparkBurst);
        VERIFY_IS_FALSE(successInactiveRefresh.fadeOut);
        VERIFY_IS_FALSE(RequiresSparkWork(successInactiveRefresh, 0));

        environment.paneActive = true;
        const auto successActiveRefresh = renderer.RefreshEnvironment(environment, RenderTimestamp{ 2500 });
        VERIFY_IS_FALSE(successActiveRefresh.successSweep);
        VERIFY_IS_FALSE(successActiveRefresh.finalSparkBurst);
        VERIFY_IS_FALSE(successActiveRefresh.fadeOut);
        VERIFY_IS_FALSE(RequiresSparkWork(successActiveRefresh, 0));

        const auto error = renderer.Apply(
            { ProgressMode::Determinate, ProgressStatus::Error, 65, true, ProgressSource::Taskbar, 4 },
            environment,
            RenderTimestamp{ 3000 });
        VERIFY_IS_TRUE(error.errorPulse);
        VERIFY_IS_FALSE(error.errorWithoutProgress);
        VERIFY_IS_FALSE(error.sparksEligible);
        VERIFY_IS_FALSE(error.fadeOut);
        VERIFY_IS_FALSE(RequiresSparkWork(error, 0));

        environment.paneActive = false;
        const auto errorInactiveRefresh = renderer.RefreshEnvironment(environment, RenderTimestamp{ 3250 });
        VERIFY_IS_FALSE(errorInactiveRefresh.errorPulse);
        VERIFY_IS_FALSE(RequiresSparkWork(errorInactiveRefresh, 0));

        environment.paneActive = true;
        const auto errorActiveRefresh = renderer.RefreshEnvironment(environment, RenderTimestamp{ 3500 });
        VERIFY_IS_FALSE(errorActiveRefresh.errorPulse);
        VERIFY_IS_FALSE(RequiresSparkWork(errorActiveRefresh, 0));

        VisualProgressRenderState unknownProgressError;
        unknownProgressError.Apply(
            { ProgressMode::Indeterminate, ProgressStatus::Running, 0, true, ProgressSource::Provider, 1 },
            environment,
            RenderTimestamp{ 0 });
        const auto statusOnlyError = unknownProgressError.Apply(
            { ProgressMode::Determinate, ProgressStatus::Error, 0, true, ProgressSource::Provider, 2 },
            environment,
            RenderTimestamp{ 100 });
        VERIFY_ARE_EQUAL(0.0f, statusOnlyError.targetProgress);
        VERIFY_IS_TRUE(statusOnlyError.errorWithoutProgress);
        VERIFY_IS_TRUE(statusOnlyError.headVisible);
        VERIFY_IS_TRUE(statusOnlyError.errorPulse);
        environment.windowFocused = false;
        const auto retainedStatusOnlyError = unknownProgressError.RefreshEnvironment(environment, RenderTimestamp{ 200 });
        VERIFY_IS_TRUE(retainedStatusOnlyError.errorWithoutProgress);
        VERIFY_IS_TRUE(retainedStatusOnlyError.headVisible);
        VERIFY_IS_FALSE(retainedStatusOnlyError.errorPulse);
        environment.windowFocused = true;

        VERIFY_ARE_EQUAL(uint32_t{ 0xFF087A63u }, RainbowArcVisualConstants::LightRunningSolid);
        VERIFY_ARE_EQUAL(uint32_t{ 0xFF8A5D00u }, RainbowArcVisualConstants::LightWaitingSolid);
        VERIFY_ARE_EQUAL(uint32_t{ 0xFF087A42u }, RainbowArcVisualConstants::LightSuccessSolid);
        VERIFY_ARE_EQUAL(uint32_t{ 0xFFB42318u }, RainbowArcVisualConstants::LightErrorSolid);

        environment.animationsEnabled = false;
        const auto reducedMotion = renderer.RefreshEnvironment(environment, RenderTimestamp{ 4000 });
        VERIFY_IS_TRUE(reducedMotion.staticFallback);
        VERIFY_IS_FALSE(reducedMotion.rainbowMoving);
        VERIFY_IS_FALSE(reducedMotion.sparksEligible);

        environment.animationsEnabled = true;
        environment.highContrast = true;
        const auto highContrast = renderer.RefreshEnvironment(environment, RenderTimestamp{ 5000 });
        VERIFY_IS_TRUE(highContrast.staticFallback);
        VERIFY_IS_FALSE(highContrast.rainbowMoving);
        VERIFY_IS_FALSE(highContrast.sparksEligible);

        const auto cancelled = renderer.Apply(
            { ProgressMode::Hidden, ProgressStatus::Cancelled, 0, false, ProgressSource::None, 5 },
            environment,
            RenderTimestamp{ 6000 });
        VERIFY_IS_TRUE(cancelled.releaseAfterTransition);
        VERIFY_IS_FALSE(cancelled.finalSparkBurst);
        VERIFY_IS_FALSE(cancelled.sparksEligible);
        VERIFY_IS_FALSE(RequiresSparkWork(cancelled, 0));
    }

    void WinTermVisualProgressTests::RendererFailureAndCloseRemainPaneLocal()
    {
        RenderEnvironment environment;
        environment.hostLoaded = true;
        environment.tabVisible = true;
        environment.paneActive = true;

        VisualProgressRenderState first;
        VisualProgressRenderState second;
        first.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 25, true, ProgressSource::Taskbar, 1 },
            environment,
            RenderTimestamp{ 0 });
        second.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 75, true, ProgressSource::Taskbar, 1 },
            environment,
            RenderTimestamp{ 0 });

        while (first.Tier() != RenderTier::Disabled)
        {
            first.Degrade(RenderTimestamp{ 1 });
        }
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::Disabled), static_cast<int>(first.Tier()));
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::Full), static_cast<int>(second.Tier()));
        VERIFY_ARE_EQUAL(0.75f, second.CurrentProgress(RenderTimestamp{ 1000 }));

        second.Close();
        VERIFY_IS_TRUE(second.Closed());
        VERIFY_IS_FALSE(second.Presenting());
        const auto afterClose = second.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 100, true, ProgressSource::Taskbar, 2 },
            environment,
            RenderTimestamp{ 2000 });
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::Disabled), static_cast<int>(afterClose.tier));
    }

    void WinTermVisualProgressTests::SparkPoolsEnforcePaneAndGlobalCaps()
    {
        SparkBudget budget;
        SparkPool first{ budget };
        SparkPool second{ budget };
        SparkPool third{ budget };
        SparkPool fourth{ budget };
        std::array<SparkHandle, RainbowArcVisualConstants::SparkPoolCapacityPerPane> firstHandles{};

        for (uint8_t index = 0; index < RainbowArcVisualConstants::SparkPoolCapacityPerPane; ++index)
        {
            const auto firstHandle = first.Acquire(RenderTimestamp{ 0 }, std::chrono::milliseconds{ 120 });
            VERIFY_IS_TRUE(firstHandle.has_value());
            firstHandles[index] = *firstHandle;
            VERIFY_IS_TRUE(second.Acquire(RenderTimestamp{ 0 }, std::chrono::milliseconds{ 180 }).has_value());
            VERIFY_IS_TRUE(third.Acquire(RenderTimestamp{ 0 }, std::chrono::milliseconds{ 260 }).has_value());
        }
        VERIFY_ARE_EQUAL(uint8_t{ 8 }, first.Live());
        VERIFY_ARE_EQUAL(uint8_t{ 24 }, budget.Live());
        VERIFY_IS_FALSE(fourth.Acquire(RenderTimestamp{ 0 }, std::chrono::milliseconds{ 120 }).has_value());

        const auto releasedHandle = firstHandles[0];
        VERIFY_IS_TRUE(first.Release(releasedHandle));
        const auto reused = fourth.Acquire(RenderTimestamp{ 1 }, std::chrono::milliseconds{ 120 });
        VERIFY_IS_TRUE(reused.has_value());
        VERIFY_ARE_EQUAL(uint8_t{ 24 }, budget.Live());

        first.ReleaseAll();
        second.ReleaseAll();
        third.ReleaseAll();
        fourth.ReleaseAll();
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, budget.Live());

        SparkBudget reuseBudget{ 1 };
        SparkPool reusePool{ reuseBudget };
        const auto original = reusePool.Acquire(RenderTimestamp{ 0 }, std::chrono::milliseconds{ 120 });
        VERIFY_IS_TRUE(original.has_value());
        VERIFY_IS_TRUE(reusePool.IsActive(*original));
        VERIFY_IS_TRUE(reusePool.Release(*original));

        const auto reacquired = reusePool.Acquire(RenderTimestamp{ 1 }, std::chrono::milliseconds{ 120 });
        VERIFY_IS_TRUE(reacquired.has_value());
        VERIFY_ARE_EQUAL(original->slot, reacquired->slot);
        VERIFY_ARE_NOT_EQUAL(original->generation, reacquired->generation);
        VERIFY_IS_FALSE(reusePool.IsActive(*original));
        VERIFY_IS_FALSE(reusePool.Release(*original));
        VERIFY_IS_TRUE(reusePool.IsActive(*reacquired));
        VERIFY_IS_TRUE(reusePool.Release(*reacquired));
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, reuseBudget.Live());

        SparkBudget expiryBudget{ 3 };
        SparkPool expiryPool{ expiryBudget };
        const auto shortLived = expiryPool.Acquire(RenderTimestamp{ 0 }, std::chrono::milliseconds{ 120 });
        const auto persistent = expiryPool.Acquire(RenderTimestamp{ 0 }, std::chrono::milliseconds{ 120 }, true);
        const auto longLived = expiryPool.Acquire(RenderTimestamp{ 100 }, std::chrono::milliseconds{ 260 });
        VERIFY_IS_TRUE(shortLived.has_value());
        VERIFY_IS_TRUE(persistent.has_value());
        VERIFY_IS_TRUE(longLived.has_value());
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, expiryPool.ReleaseExpired(RenderTimestamp{ 119 }));
        VERIFY_ARE_EQUAL(uint8_t{ 1 }, expiryPool.ReleaseExpired(RenderTimestamp{ 120 }));
        VERIFY_IS_FALSE(expiryPool.IsActive(*shortLived));
        VERIFY_IS_TRUE(expiryPool.IsActive(*persistent));
        VERIFY_IS_TRUE(expiryPool.IsActive(*longLived));
        VERIFY_ARE_EQUAL(uint8_t{ 1 }, expiryPool.ReleaseExpired(RenderTimestamp::max()));
        VERIFY_IS_TRUE(expiryPool.IsActive(*persistent));
        VERIFY_ARE_EQUAL(uint8_t{ 1 }, expiryPool.Live());
        VERIFY_ARE_EQUAL(uint8_t{ 1 }, expiryBudget.Live());
        VERIFY_IS_TRUE(expiryPool.Release(*persistent));
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, expiryBudget.Live());

        SparkBudget scopedBudget{ 4 };
        {
            SparkPool scopedPool{ scopedBudget };
            VERIFY_IS_TRUE(scopedPool.Acquire(RenderTimestamp{ 0 }, std::chrono::milliseconds{ 120 }).has_value());
            VERIFY_IS_TRUE(scopedPool.Acquire(RenderTimestamp{ 0 }, std::chrono::milliseconds{ 180 }, true).has_value());
            VERIFY_ARE_EQUAL(uint8_t{ 2 }, scopedBudget.Live());
        }
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, scopedBudget.Live());
    }

    void WinTermVisualProgressTests::BackgroundAndHiddenPanesDoNotRequestSparkWork()
    {
        RenderEnvironment environment;
        environment.hostLoaded = true;
        environment.tabVisible = true;
        environment.paneActive = false;

        VisualProgressRenderState renderer;
        const auto background = renderer.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 50, true, ProgressSource::Taskbar, 1 },
            environment,
            RenderTimestamp{ 0 });
        VERIFY_IS_FALSE(background.sparksEligible);
        VERIFY_IS_FALSE(RequiresSparkWork(background, 0));

        environment.paneActive = true;
        environment.windowFocused = false;
        const auto unfocused = renderer.RefreshEnvironment(environment, RenderTimestamp{ 1 });
        VERIFY_IS_TRUE(unfocused.visible);
        VERIFY_IS_FALSE(unfocused.rainbowMoving);
        VERIFY_IS_FALSE(unfocused.sparksEligible);
        VERIFY_IS_FALSE(RequiresSparkWork(unfocused, 0));

        environment.windowFocused = true;
        const auto refocused = renderer.RefreshEnvironment(environment, RenderTimestamp{ 2 });
        VERIFY_IS_TRUE(refocused.visible);
        VERIFY_IS_TRUE(refocused.rainbowMoving);
        VERIFY_IS_TRUE(refocused.sparksEligible);

        environment.windowVisible = false;
        const auto hidden = renderer.RefreshEnvironment(environment, RenderTimestamp{ 3 });
        VERIFY_IS_FALSE(hidden.visible);
        VERIFY_IS_FALSE(hidden.sparksEligible);
        VERIFY_IS_FALSE(RequiresSparkWork(hidden, 0));
        VERIFY_IS_TRUE(RequiresSparkWork(hidden, 1));

        environment.windowVisible = true;
        environment.tabVisible = false;
        const auto backgroundTab = renderer.RefreshEnvironment(environment, RenderTimestamp{ 4 });
        VERIFY_IS_FALSE(backgroundTab.visible);
        VERIFY_IS_FALSE(backgroundTab.sparksEligible);
        VERIFY_IS_FALSE(RequiresSparkWork(backgroundTab, 0));
    }

    void WinTermVisualProgressTests::CommandCompletionClearsAtNextPrompt()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        // Composing input at the prompt is not execution: 133;B must not
        // show a bar, so an idle shell-integrated pane stays quiet.
        VERIFY_IS_FALSE(state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1).has_value());
        VERIFY_IS_FALSE(state.Current().visible);

        auto snapshot = state.ApplyShellLifecycle(ShellLifecycleState::CommandExecuted, -1);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(snapshot->mode));

        snapshot = state.ApplyShellLifecycle(ShellLifecycleState::CommandFinished, 0);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Success), static_cast<int>(snapshot->status));
        VERIFY_ARE_EQUAL(uint8_t{ 100 }, snapshot->value);

        snapshot = state.ApplyShellLifecycle(ShellLifecycleState::Prompt, -1);
        VERIFY_IS_FALSE(snapshot->visible);

        // The next idle prompt keeps the bar hidden even after 133;B.
        VERIFY_IS_FALSE(state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1).has_value());
        VERIFY_IS_FALSE(state.Current().visible);
    }

    void WinTermVisualProgressTests::EmergencyOverridePrecedesSetting()
    {
        VERIFY_IS_TRUE(IsFeatureEnabled(true, L""));
        VERIFY_IS_TRUE(IsFeatureEnabled(true, L"0"));
        VERIFY_IS_FALSE(IsFeatureEnabled(true, L"1"));
        VERIFY_IS_FALSE(IsFeatureEnabled(false, L""));
    }

    void WinTermVisualProgressTests::DisabledFeatureIgnoresEvents()
    {
        ProgressStateMachine state;
        VERIFY_IS_FALSE(state.ApplyTaskbar(1, 50).has_value());
        VERIFY_IS_FALSE(state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1).has_value());
        VERIFY_IS_FALSE(state.ApplyProvider({
                                                ProgressProvider::Generic,
                                                ProgressMode::Determinate,
                                                ProgressStatus::Running,
                                                50,
                                                ProviderConfidence::Medium,
                                                true,
                                                true,
                                                false,
                                                0,
                                                1,
                                            })
                            .has_value());
        VERIFY_IS_FALSE(state.Current().visible);
    }

    void WinTermVisualProgressTests::MultiplePanesRemainIndependent()
    {
        ProgressStateMachine first;
        ProgressStateMachine second;
        first.SetEnabled(true);
        second.SetEnabled(true);
        first.ApplyTaskbar(1, 25);
        second.ApplyTaskbar(1, 75);
        VERIFY_ARE_EQUAL(uint8_t{ 25 }, first.Current().value);
        VERIFY_ARE_EQUAL(uint8_t{ 75 }, second.Current().value);
    }

    void WinTermVisualProgressTests::CloseAndDetachCleanupStopsUpdates()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        state.ApplyTaskbar(1, 25);
        const auto closed = state.Close();
        VERIFY_IS_TRUE(closed.has_value());
        VERIFY_IS_FALSE(closed->visible);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Cancelled), static_cast<int>(closed->status));
        VERIFY_IS_FALSE(state.ApplyTaskbar(1, 75).has_value());
        VERIFY_IS_FALSE(state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1).has_value());
    }

    void WinTermVisualProgressTests::SplitOrDetachResetClearsReusableState()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        state.ApplyTaskbar(1, 25);

        const auto reset = state.Reset();
        VERIFY_IS_TRUE(reset.has_value());
        VERIFY_IS_FALSE(reset->visible);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Hidden), static_cast<int>(reset->mode));

        const auto reused = state.ApplyShellLifecycle(ShellLifecycleState::CommandExecuted, -1);
        VERIFY_IS_TRUE(reused.has_value());
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(reused->mode));
    }

    void WinTermVisualProgressTests::FeatureReloadDisablesAndReenablesCleanly()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        state.ApplyTaskbar(1, 50);

        const auto disabled = state.SetEnabled(false);
        VERIFY_IS_TRUE(disabled.has_value());
        VERIFY_IS_FALSE(disabled->visible);
        VERIFY_IS_FALSE(state.ApplyTaskbar(1, 75).has_value());

        state.SetEnabled(true);
        const auto reenabled = state.ApplyTaskbar(1, 75);
        VERIFY_IS_TRUE(reenabled.has_value());
        VERIFY_ARE_EQUAL(uint8_t{ 75 }, reenabled->value);
    }

    void WinTermVisualProgressTests::MailboxCoalescesRapidUpdatesAndReleasesOnClose()
    {
        ProgressUpdateMailbox mailbox;
        bool allPublished = true;
        for (uint64_t value = 0; value < 10000; ++value)
        {
            allPublished = mailbox.Publish({ ProgressMode::Determinate, ProgressStatus::Running, static_cast<uint8_t>(value % 101), true, ProgressSource::Taskbar, value }) && allPublished;
        }
        VERIFY_IS_TRUE(allPublished);
        const auto latest = mailbox.TakeLatest();
        VERIFY_IS_TRUE(latest.has_value());
        VERIFY_ARE_EQUAL(uint64_t{ 9999 }, latest->sequence);
        VERIFY_IS_FALSE(mailbox.HasPending());

        mailbox.Close();
        VERIFY_IS_FALSE(mailbox.Publish({ ProgressMode::Indeterminate, ProgressStatus::Running, 0, true, ProgressSource::Taskbar, 10000 }));
        VERIFY_IS_FALSE(mailbox.TakeLatest().has_value());
    }

    void WinTermVisualProgressTests::UnexpectedEventsFailOpen()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        VERIFY_IS_FALSE(state.ApplyTaskbar(99, UINT64_MAX).has_value());
        VERIFY_IS_FALSE(state.ApplyShellLifecycle(static_cast<ShellLifecycleState>(99), INT64_MAX).has_value());
        VERIFY_IS_FALSE(state.Current().visible);
    }

    void WinTermVisualProgressTests::SettingsUseStableDefaultsAndRoundTrip()
    {
        Json::Value missing{ Json::objectValue };
        const auto defaults = winrt::Microsoft::Terminal::Settings::Model::implementation::GlobalAppSettings::FromJson(missing);
        VERIFY_IS_TRUE(defaults->VisualProgressEnabled());
        VERIFY_IS_TRUE(defaults->VisualProgressRecognizeCliProgress());
        VERIFY_ARE_EQUAL(
            static_cast<int>(VisualProgressPerformanceMode::Automatic),
            static_cast<int>(defaults->VisualProgressPerformanceMode()));
        VERIFY_IS_FALSE(defaults->VisualProgressReplaceRecognizedOutput());
        VERIFY_IS_FALSE(defaults->ToJson().isMember("visualProgress.enabled"));
        VERIFY_IS_FALSE(defaults->ToJson().isMember("visualProgress.recognizeCliProgress"));
        VERIFY_IS_FALSE(defaults->ToJson().isMember("visualProgress.performanceMode"));
        VERIFY_IS_FALSE(defaults->ToJson().isMember("visualProgress.replaceRecognizedOutput"));

        Json::Value explicitValues{ Json::objectValue };
        explicitValues["visualProgress.enabled"] = false;
        explicitValues["visualProgress.recognizeCliProgress"] = false;
        explicitValues["visualProgress.performanceMode"] = "balanced";
        explicitValues["visualProgress.replaceRecognizedOutput"] = true;
        const auto explicitSettings = winrt::Microsoft::Terminal::Settings::Model::implementation::GlobalAppSettings::FromJson(explicitValues);
        VERIFY_IS_FALSE(explicitSettings->VisualProgressEnabled());
        VERIFY_IS_FALSE(explicitSettings->VisualProgressRecognizeCliProgress());
        VERIFY_ARE_EQUAL(
            static_cast<int>(VisualProgressPerformanceMode::Balanced),
            static_cast<int>(explicitSettings->VisualProgressPerformanceMode()));
        VERIFY_IS_TRUE(explicitSettings->VisualProgressReplaceRecognizedOutput());
        const auto serialized = explicitSettings->ToJson();
        VERIFY_IS_FALSE(serialized["visualProgress.enabled"].asBool());
        VERIFY_IS_FALSE(serialized["visualProgress.recognizeCliProgress"].asBool());
        VERIFY_ARE_EQUAL(std::string{ "balanced" }, serialized["visualProgress.performanceMode"].asString());
        VERIFY_IS_TRUE(serialized["visualProgress.replaceRecognizedOutput"].asBool());

        struct ModeFixture
        {
            const char* jsonValue;
            VisualProgressPerformanceMode expected;
        };
        for (const auto& fixture : {
                 ModeFixture{ "automatic", VisualProgressPerformanceMode::Automatic },
                 ModeFixture{ "full", VisualProgressPerformanceMode::Full },
                 ModeFixture{ "balanced", VisualProgressPerformanceMode::Balanced },
                 ModeFixture{ "minimal", VisualProgressPerformanceMode::Minimal },
             })
        {
            Json::Value json{ Json::objectValue };
            json["visualProgress.performanceMode"] = fixture.jsonValue;
            const auto settings = winrt::Microsoft::Terminal::Settings::Model::implementation::GlobalAppSettings::FromJson(json);
            VERIFY_ARE_EQUAL(static_cast<int>(fixture.expected), static_cast<int>(settings->VisualProgressPerformanceMode()));
            VERIFY_ARE_EQUAL(std::string{ fixture.jsonValue }, settings->ToJson()["visualProgress.performanceMode"].asString());
        }

        Json::Value invalid{ Json::objectValue };
        invalid["visualProgress.performanceMode"] = "maximum";
        VERIFY_THROWS(
            winrt::Microsoft::Terminal::Settings::Model::implementation::GlobalAppSettings::FromJson(invalid),
            std::exception);
    }

    void WinTermVisualProgressTests::GovernorAppliesModesAndEnvironmentCaps()
    {
        PerformanceGovernorInputs inputs;
        inputs.progressVisible = true;
        inputs.progressActive = true;
        inputs.visibleActiveProgressCount = 1;

        VisualProgressPerformanceGovernor governor;
        auto decision = governor.Evaluate(inputs);
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::Full), static_cast<int>(decision.tier));
        VERIFY_IS_TRUE(decision.continuousAnimation);
        VERIFY_IS_TRUE(decision.sparks);
        VERIFY_IS_TRUE(decision.shouldSample);

        inputs.visibleActiveProgressCount = 2;
        decision = governor.Evaluate(inputs);
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::NoSparks), static_cast<int>(decision.tier));
        VERIFY_IS_FALSE(decision.sparks);
        inputs.visibleActiveProgressCount = 4;
        decision = governor.Evaluate(inputs);
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::StaticGradient), static_cast<int>(decision.tier));
        VERIFY_IS_FALSE(decision.continuousAnimation);

        inputs.visibleActiveProgressCount = 1;
        inputs.mode = PerformanceMode::Balanced;
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::NoSparks), static_cast<int>(governor.Evaluate(inputs).tier));
        inputs.mode = PerformanceMode::Minimal;
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::StaticGradient), static_cast<int>(governor.Evaluate(inputs).tier));

        inputs.mode = PerformanceMode::Full;
        inputs.highContrast = true;
        decision = governor.Evaluate(inputs);
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::Solid), static_cast<int>(decision.tier));
        VERIFY_IS_FALSE(decision.continuousAnimation);
        VERIFY_IS_FALSE(decision.sparks);

        inputs.highContrast = false;
        inputs.osAnimationsEnabled = false;
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::StaticGradient), static_cast<int>(governor.Evaluate(inputs).tier));
        inputs.osAnimationsEnabled = true;
        inputs.applicationAnimationsEnabled = false;
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::StaticGradient), static_cast<int>(governor.Evaluate(inputs).tier));
        inputs.applicationAnimationsEnabled = true;
        inputs.softwareRendering = true;
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::StaticGradient), static_cast<int>(governor.Evaluate(inputs).tier));
        inputs.softwareRendering = false;
        inputs.remoteSession = true;
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::StaticGradient), static_cast<int>(governor.Evaluate(inputs).tier));
        inputs.remoteSession = false;
        inputs.energySaver = true;
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::StaticGradient), static_cast<int>(governor.Evaluate(inputs).tier));
        inputs.energySaver = false;
        inputs.effectsFast = false;
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::NoSparks), static_cast<int>(governor.Evaluate(inputs).tier));

        inputs.effectsFast = true;
        inputs.paneActive = false;
        decision = governor.Evaluate(inputs);
        VERIFY_IS_FALSE(decision.continuousAnimation);
        VERIFY_IS_FALSE(decision.sparks);
        VERIFY_IS_FALSE(decision.shouldSample);
        inputs.paneActive = true;
        inputs.windowFocused = false;
        decision = governor.Evaluate(inputs);
        VERIFY_IS_FALSE(decision.continuousAnimation);
        VERIFY_IS_FALSE(decision.sparks);
        VERIFY_IS_FALSE(decision.shouldSample);

        inputs.windowFocused = true;
        inputs.windowVisible = false;
        decision = governor.Evaluate(inputs);
        VERIFY_IS_FALSE(decision.present);
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::Disabled), static_cast<int>(decision.tier));
        VERIFY_IS_FALSE(decision.shouldSample);
        inputs.windowVisible = true;
        inputs.windowMinimized = true;
        VERIFY_IS_FALSE(governor.Evaluate(inputs).shouldSample);
        inputs.windowMinimized = false;
        inputs.featureEnabled = false;
        VERIFY_IS_FALSE(governor.Evaluate(inputs).present);
        inputs.featureEnabled = true;
        inputs.emergencyDisabled = true;
        VERIFY_IS_FALSE(governor.Evaluate(inputs).present);
    }

    void WinTermVisualProgressTests::GovernorHysteresisIsBoundedAndDeterministic()
    {
        PerformanceGovernorInputs inputs;
        inputs.progressVisible = true;
        inputs.progressActive = true;
        inputs.visibleActiveProgressCount = 1;

        VisualProgressPerformanceGovernor governor;
        auto decision = governor.ObserveDispatchLatency(inputs, std::chrono::milliseconds{ 100 }, PerformanceTimestamp{ 1000 });
        VERIFY_IS_TRUE(decision.observationAccepted);
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::Full), static_cast<int>(decision.tier));
        decision = governor.ObserveDispatchLatency(inputs, std::chrono::milliseconds{ 100 }, PerformanceTimestamp{ 1500 });
        VERIFY_IS_FALSE(decision.observationAccepted);
        decision = governor.ObserveDispatchLatency(inputs, std::chrono::milliseconds{ 100 }, PerformanceTimestamp{ 2000 });
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::Full), static_cast<int>(decision.tier));
        decision = governor.ObserveDispatchLatency(inputs, std::chrono::milliseconds{ 100 }, PerformanceTimestamp{ 3000 });
        VERIFY_IS_TRUE(decision.tierChanged);
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::NoSparks), static_cast<int>(decision.tier));

        governor.ObserveDispatchLatency(inputs, std::chrono::milliseconds{ 100 }, PerformanceTimestamp{ 4000 });
        governor.ObserveDispatchLatency(inputs, std::chrono::milliseconds{ 100 }, PerformanceTimestamp{ 5000 });
        decision = governor.ObserveDispatchLatency(inputs, std::chrono::milliseconds{ 100 }, PerformanceTimestamp{ 6000 });
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::StaticGradient), static_cast<int>(decision.tier));

        for (uint64_t sample = 1; sample <= 15; ++sample)
        {
            decision = governor.ObserveDispatchLatency(
                inputs,
                std::chrono::milliseconds{ 40 },
                PerformanceTimestamp{ 6000 + (sample * 1000) });
        }
        VERIFY_IS_TRUE(decision.tierChanged);
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::NoSparks), static_cast<int>(decision.tier));

        decision = governor.ObserveHardFailure(inputs, PerformanceTimestamp{ 22000 });
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::StaticGradient), static_cast<int>(decision.tier));
        VERIFY_IS_TRUE(decision.tierChanged);

        // Dispatch latency alone may reduce Automatic through Solid, but it
        // must never enter the hard-failure-only Disabled tier and strand the
        // sampler. Sustained healthy observations can therefore recover it.
        VisualProgressPerformanceGovernor latencyFloor;
        for (uint64_t sample = 1; sample <= 12; ++sample)
        {
            decision = latencyFloor.ObserveDispatchLatency(
                inputs,
                std::chrono::milliseconds{ 100 },
                PerformanceTimestamp{ sample * 1000 });
        }
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::Solid), static_cast<int>(latencyFloor.AdaptiveTier()));
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::Solid), static_cast<int>(decision.tier));
        VERIFY_IS_TRUE(decision.shouldSample);

        for (uint64_t sample = 1; sample <= 15; ++sample)
        {
            decision = latencyFloor.ObserveDispatchLatency(
                inputs,
                std::chrono::milliseconds{ 40 },
                PerformanceTimestamp{ 22000 + (sample * 1000) });
        }
        VERIFY_IS_TRUE(decision.tierChanged);
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::StaticGradient), static_cast<int>(latencyFloor.AdaptiveTier()));

        inputs.visibleActiveProgressCount = 0;
        decision = governor.Evaluate(inputs);
        VERIFY_IS_FALSE(decision.shouldSample);
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::Full), static_cast<int>(governor.AdaptiveTier()));

        VisualProgressPerformanceGovernor independentWindow;
        inputs.visibleActiveProgressCount = 1;
        VERIFY_ARE_EQUAL(static_cast<int>(RenderTier::Full), static_cast<int>(independentWindow.Evaluate(inputs).tier));
    }

    void WinTermVisualProgressTests::AccessibilitySemanticsAndAnnouncementsAreBounded()
    {
        VisualProgressAccessibilityPolicy policy;
        auto update = policy.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 0, true, ProgressSource::Taskbar, 1 },
            true,
            true,
            AccessibilityTimestamp{ 0 });
        VERIFY_IS_TRUE(update.semantics.visible);
        VERIFY_IS_TRUE(update.semantics.hasNumericValue);
        VERIFY_ARE_EQUAL(uint8_t{ 0 }, update.semantics.value);
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::Started), static_cast<int>(update.announcement));

        update = policy.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 25, true, ProgressSource::Taskbar, 2 },
            true,
            true,
            AccessibilityTimestamp{ 1000 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::None), static_cast<int>(update.announcement));
        update = policy.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 30, true, ProgressSource::Taskbar, 3 },
            true,
            true,
            AccessibilityTimestamp{ 4000 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::Progress25), static_cast<int>(update.announcement));

        update = policy.Apply(
            { ProgressMode::Determinate, ProgressStatus::Waiting, 50, true, ProgressSource::Taskbar, 4 },
            true,
            true,
            AccessibilityTimestamp{ 5000 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::None), static_cast<int>(update.announcement));
        update = policy.Apply(
            { ProgressMode::Determinate, ProgressStatus::Waiting, 50, true, ProgressSource::Taskbar, 5 },
            true,
            true,
            AccessibilityTimestamp{ 8000 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::Waiting), static_cast<int>(update.announcement));

        update = policy.Apply(
            { ProgressMode::Determinate, ProgressStatus::Success, 100, true, ProgressSource::Taskbar, 6 },
            true,
            true,
            AccessibilityTimestamp{ 8001 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::Success), static_cast<int>(update.announcement));
        update = policy.Apply(
            { ProgressMode::Determinate, ProgressStatus::Success, 100, true, ProgressSource::Taskbar, 7 },
            true,
            true,
            AccessibilityTimestamp{ 8002 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::None), static_cast<int>(update.announcement));

        policy.Reset();
        update = policy.Apply(
            { ProgressMode::Indeterminate, ProgressStatus::Running, 0, true, ProgressSource::ShellIntegration, 1 },
            true,
            true,
            AccessibilityTimestamp{ 0 });
        VERIFY_IS_TRUE(update.semantics.IsIndeterminate());
        VERIFY_IS_FALSE(update.semantics.hasNumericValue);
        update = policy.Apply(
            { ProgressMode::Indeterminate, ProgressStatus::Error, 0, true, ProgressSource::ShellIntegration, 2 },
            true,
            true,
            AccessibilityTimestamp{ 1 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::Error), static_cast<int>(update.announcement));

        policy.Reset();
        policy.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 25, true, ProgressSource::Provider, 1 },
            true,
            false,
            AccessibilityTimestamp{ 0 });
        update = policy.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 25, true, ProgressSource::Provider, 2 },
            true,
            true,
            AccessibilityTimestamp{ 5000 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::None), static_cast<int>(update.announcement));
        update = policy.Apply(
            { ProgressMode::Hidden, ProgressStatus::Cancelled, 0, false, ProgressSource::None, 3 },
            true,
            true,
            AccessibilityTimestamp{ 6000 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::None), static_cast<int>(update.announcement));

        policy.Reset();
        policy.Apply(
            { ProgressMode::Determinate, ProgressStatus::Running, 10, true, ProgressSource::Taskbar, 1 },
            true,
            true,
            AccessibilityTimestamp{ 0 });
        update = policy.Apply(
            { ProgressMode::Hidden, ProgressStatus::Cancelled, 0, false, ProgressSource::None, 2 },
            true,
            true,
            AccessibilityTimestamp{ 1 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::Cancelled), static_cast<int>(update.announcement));

        policy.Reset();
        update = policy.Apply(
            { ProgressMode::Indeterminate, ProgressStatus::Waiting, 0, true, ProgressSource::ShellIntegration, 1 },
            true,
            true,
            AccessibilityTimestamp{ 0 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::Started), static_cast<int>(update.announcement));
        update = policy.Apply(
            { ProgressMode::Indeterminate, ProgressStatus::Waiting, 0, true, ProgressSource::ShellIntegration, 2 },
            true,
            true,
            AccessibilityTimestamp{ 4000 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::Waiting), static_cast<int>(update.announcement));

        policy.Reset();
        update = policy.Apply(
            { ProgressMode::Indeterminate, ProgressStatus::Waiting, 0, true, ProgressSource::ShellIntegration, 1 },
            true,
            true,
            AccessibilityTimestamp{ 0 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::Started), static_cast<int>(update.announcement));
        VERIFY_IS_TRUE(policy.HasPendingAnnouncement());
        update = policy.Apply(
            { ProgressMode::Indeterminate, ProgressStatus::Running, 0, true, ProgressSource::ShellIntegration, 2 },
            true,
            true,
            AccessibilityTimestamp{ 1000 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::None), static_cast<int>(update.announcement));
        VERIFY_IS_FALSE(policy.HasPendingAnnouncement());
        update = policy.Apply(
            { ProgressMode::Indeterminate, ProgressStatus::Running, 0, true, ProgressSource::ShellIntegration, 3 },
            true,
            true,
            AccessibilityTimestamp{ 4000 });
        VERIFY_ARE_EQUAL(static_cast<int>(AccessibilityAnnouncement::None), static_cast<int>(update.announcement));
    }

    void WinTermVisualProgressTests::Phase3StressCoverageRemainsBounded()
    {
        ProgressStateMachine model;
        model.SetEnabled(true);
        bool modelHealthy = true;
        for (uint64_t update = 0; update < 100000; ++update)
        {
            const auto snapshot = model.ApplyTaskbar(1, update % 101);
            modelHealthy = modelHealthy && snapshot.has_value();
        }
        VERIFY_IS_TRUE(modelHealthy);
        VERIFY_ARE_EQUAL(uint8_t{ 9 }, model.Current().value);

        RecognitionEngine recognizer;
        bool recognizedHealthy = true;
        for (uint64_t record = 0; record < 100000; ++record)
        {
            const auto result = recognizer.Consume(
                L"Receiving objects: 50% (50/100)\n",
                record * RecognitionEngine::PublicationIntervalMilliseconds);
            recognizedHealthy = recognizedHealthy && result.accepted && result.healthy && !result.overflow && !result.suppressInput;
            if ((record + 1) % 1000 == 0)
            {
                recognizer.Reset();
            }
        }
        VERIFY_IS_TRUE(recognizedHealthy);

        std::array<ProgressStateMachine, 4> panes;
        std::array<std::thread, 4> workers;
        std::atomic<bool> concurrentHealthy{ true };
        for (auto& pane : panes)
        {
            pane.SetEnabled(true);
        }
        for (size_t paneIndex = 0; paneIndex < panes.size(); ++paneIndex)
        {
            workers[paneIndex] = std::thread{ [&, paneIndex]() {
                for (uint64_t update = 0; update < 25000; ++update)
                {
                    if (!panes[paneIndex].ApplyTaskbar(1, (update + paneIndex) % 101).has_value())
                    {
                        concurrentHealthy.store(false, std::memory_order_release);
                    }
                }
            } };
        }
        for (auto& worker : workers)
        {
            worker.join();
        }
        VERIFY_IS_TRUE(concurrentHealthy.load(std::memory_order_acquire));

        RenderEnvironment renderEnvironment;
        renderEnvironment.hostLoaded = true;
        renderEnvironment.tabVisible = true;
        renderEnvironment.paneActive = true;
        bool lifecycleHealthy = true;
        bool providerLifecycleHealthy = true;
        bool splitCloseRehydrationHealthy = true;
        bool focusVisibilityHealthy = true;
        bool repeatedTerminalStatesHealthy = true;
        bool rendererRecoveryHealthy = true;
        bool governorRecoveryHealthy = true;
        bool samplerLifecycleHealthy = true;
        for (uint32_t lifecycle = 0; lifecycle < 10000; ++lifecycle)
        {
            auto& pane = panes[lifecycle % panes.size()];
            pane.SetEnabled(false);
            lifecycleHealthy = lifecycleHealthy && !pane.ApplyTaskbar(1, 50).has_value();
            pane.SetEnabled(true);

            const ProviderProgress provider{
                ProgressProvider::Git,
                ProgressMode::Determinate,
                ProgressStatus::Running,
                static_cast<uint8_t>(lifecycle % 101),
                ProviderConfidence::High,
                true,
                true,
                true,
                2,
                lifecycle,
            };
            const auto claimed = pane.ApplyProvider(provider);
            const auto released = pane.ApplyProvider({});
            providerLifecycleHealthy = providerLifecycleHealthy &&
                                       claimed.has_value() &&
                                       claimed->source == ProgressSource::Provider &&
                                       released.has_value() &&
                                       !released->visible;

            // Model the reset/reparent/rehydrate sequence used when a split
            // child closes. Re-applying current sources in precedence order
            // must restore the survivor without scanning scrollback.
            pane.Reset();
            pane.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1);
            pane.ApplyTaskbar(0, 0);
            const auto rehydrated = pane.ApplyProvider(provider);
            splitCloseRehydrationHealthy = splitCloseRehydrationHealthy &&
                                           rehydrated.has_value() &&
                                           rehydrated->source == ProgressSource::Provider;
            lifecycleHealthy = lifecycleHealthy && pane.ApplyTaskbar(1, lifecycle % 101).has_value();

            VisualProgressRenderState renderer;
            const auto running = renderer.Apply(
                { ProgressMode::Determinate, ProgressStatus::Running, static_cast<uint8_t>(lifecycle % 101), true, ProgressSource::Taskbar, 1 },
                renderEnvironment,
                RenderTimestamp{ 0 });
            renderEnvironment.windowFocused = false;
            const auto unfocused = renderer.RefreshEnvironment(renderEnvironment, RenderTimestamp{ 250 });
            renderEnvironment.windowFocused = true;
            renderEnvironment.windowVisible = false;
            const auto hidden = renderer.RefreshEnvironment(renderEnvironment, RenderTimestamp{ 500 });
            renderEnvironment.windowVisible = true;
            const auto refocused = renderer.RefreshEnvironment(renderEnvironment, RenderTimestamp{ 750 });
            focusVisibilityHealthy = focusVisibilityHealthy &&
                                     !unfocused.rainbowMoving &&
                                     !hidden.visible &&
                                     refocused.visible;

            const auto terminalStatus = lifecycle % 2 == 0 ? ProgressStatus::Success : ProgressStatus::Error;
            const auto terminal = renderer.Apply(
                { ProgressMode::Determinate, terminalStatus, static_cast<uint8_t>(terminalStatus == ProgressStatus::Success ? 100 : lifecycle % 101), true, ProgressSource::Taskbar, 2 },
                renderEnvironment,
                RenderTimestamp{ 1000 });
            repeatedTerminalStatesHealthy = repeatedTerminalStatesHealthy &&
                                            running.visible &&
                                            terminal.visible &&
                                            terminal.releaseAfterTransition == (terminalStatus == ProgressStatus::Success);

            // Degrading to Disabled simulates an exhausted device/render fault.
            // A new pane lifecycle is the bounded recovery boundary.
            while (renderer.Tier() != RenderTier::Disabled)
            {
                renderer.Degrade(RenderTimestamp{ 2000 });
            }
            VisualProgressRenderState recoveredRenderer;
            const auto recovered = recoveredRenderer.Apply(
                { ProgressMode::Determinate, ProgressStatus::Running, 50, true, ProgressSource::Taskbar, 1 },
                renderEnvironment,
                RenderTimestamp{ 3000 });
            rendererRecoveryHealthy = rendererRecoveryHealthy &&
                                      renderer.Tier() == RenderTier::Disabled &&
                                      recovered.visible &&
                                      recoveredRenderer.Tier() == RenderTier::Full;

            PerformanceGovernorInputs inputs;
            inputs.progressVisible = true;
            inputs.progressActive = true;
            inputs.visibleActiveProgressCount = static_cast<uint16_t>(panes.size());
            VisualProgressPerformanceGovernor governor;
            governor.ObserveHardFailure(inputs, PerformanceTimestamp{ 0 });
            inputs.visibleActiveProgressCount = 0;
            governor.Evaluate(inputs);
            governorRecoveryHealthy = governorRecoveryHealthy && governor.AdaptiveTier() == RenderTier::Full;

            VisualProgressSamplerState sampler;
            uint64_t staleGeneration{};
            samplerLifecycleHealthy = samplerLifecycleHealthy &&
                                      sampler.Start() &&
                                      sampler.TryBeginProbe(staleGeneration) &&
                                      !sampler.TryBeginProbe(staleGeneration);
            sampler.Stop();
            samplerLifecycleHealthy = samplerLifecycleHealthy &&
                                      !sampler.TryCompleteProbe(staleGeneration) &&
                                      sampler.Start();
            uint64_t currentGeneration{};
            samplerLifecycleHealthy = samplerLifecycleHealthy &&
                                      sampler.TryBeginProbe(currentGeneration) &&
                                      sampler.TryCompleteProbe(currentGeneration) &&
                                      sampler.Close() &&
                                      !sampler.Start();
        }
        VERIFY_IS_TRUE(lifecycleHealthy);
        VERIFY_IS_TRUE(providerLifecycleHealthy);
        VERIFY_IS_TRUE(splitCloseRehydrationHealthy);
        VERIFY_IS_TRUE(focusVisibilityHealthy);
        VERIFY_IS_TRUE(repeatedTerminalStatesHealthy);
        VERIFY_IS_TRUE(rendererRecoveryHealthy);
        VERIFY_IS_TRUE(governorRecoveryHealthy);
        VERIFY_IS_TRUE(samplerLifecycleHealthy);
    }
}
