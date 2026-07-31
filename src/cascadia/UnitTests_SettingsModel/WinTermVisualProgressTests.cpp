// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalSettingsModel/GlobalAppSettings.h"
#include "../../winterm/VisualProgress/ProgressRecognition.h"
#include "../../winterm/VisualProgress/VisualProgressModel.h"
#include "../../winterm/VisualProgress/VisualProgressRenderModel.h"

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
        TEST_METHOD(SettingSerializesAndMissingSettingDefaultsOff);
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
        const auto backupResult = unrelatedDownload.Consume(L"Downloading backup 50% 1MB/2MB 1MB/s eta 1s\r\x1b[2K", 50, replacement);
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
        environment.windowVisible = false;
        const auto hidden = renderer.RefreshEnvironment(environment, RenderTimestamp{ 1 });
        VERIFY_IS_FALSE(hidden.visible);
        VERIFY_IS_FALSE(hidden.sparksEligible);
        VERIFY_IS_FALSE(RequiresSparkWork(hidden, 0));
        VERIFY_IS_TRUE(RequiresSparkWork(hidden, 1));

        environment.windowVisible = true;
        environment.tabVisible = false;
        const auto backgroundTab = renderer.RefreshEnvironment(environment, RenderTimestamp{ 2 });
        VERIFY_IS_FALSE(backgroundTab.visible);
        VERIFY_IS_FALSE(backgroundTab.sparksEligible);
        VERIFY_IS_FALSE(RequiresSparkWork(backgroundTab, 0));
    }

    void WinTermVisualProgressTests::CommandCompletionClearsAtNextPrompt()
    {
        ProgressStateMachine state;
        state.SetEnabled(true);
        auto snapshot = state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressMode::Indeterminate), static_cast<int>(snapshot->mode));

        snapshot = state.ApplyShellLifecycle(ShellLifecycleState::CommandFinished, 0);
        VERIFY_ARE_EQUAL(static_cast<int>(ProgressStatus::Success), static_cast<int>(snapshot->status));
        VERIFY_ARE_EQUAL(uint8_t{ 100 }, snapshot->value);

        snapshot = state.ApplyShellLifecycle(ShellLifecycleState::Prompt, -1);
        VERIFY_IS_FALSE(snapshot->visible);
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

        const auto reused = state.ApplyShellLifecycle(ShellLifecycleState::CommandStart, -1);
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

    void WinTermVisualProgressTests::SettingSerializesAndMissingSettingDefaultsOff()
    {
        Json::Value enabledJson{ Json::objectValue };
        enabledJson["visualProgress.enabled"] = true;
        enabledJson["visualProgress.replaceRecognizedOutput"] = true;
        const auto enabled = winrt::Microsoft::Terminal::Settings::Model::implementation::GlobalAppSettings::FromJson(enabledJson);
        VERIFY_IS_TRUE(enabled->VisualProgressEnabled());
        VERIFY_IS_TRUE(enabled->VisualProgressReplaceRecognizedOutput());
        VERIFY_IS_TRUE(enabled->ToJson()["visualProgress.enabled"].asBool());
        VERIFY_IS_TRUE(enabled->ToJson()["visualProgress.replaceRecognizedOutput"].asBool());

        Json::Value legacyJson{ Json::objectValue };
        const auto migrated = winrt::Microsoft::Terminal::Settings::Model::implementation::GlobalAppSettings::FromJson(legacyJson);
        VERIFY_IS_FALSE(migrated->VisualProgressEnabled());
        VERIFY_IS_FALSE(migrated->VisualProgressReplaceRecognizedOutput());
        VERIFY_IS_FALSE(migrated->ToJson().isMember("visualProgress.enabled"));
        VERIFY_IS_FALSE(migrated->ToJson().isMember("visualProgress.replaceRecognizedOutput"));
    }
}
