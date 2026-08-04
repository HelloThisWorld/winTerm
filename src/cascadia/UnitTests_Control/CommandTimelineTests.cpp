// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../TerminalControl/ControlCore.h"
#include "../../winterm/CommandTimeline/CommandTimelineModel.h"
#include "MockConnection.h"
#include "MockControlSettings.h"

using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace winTerm::CommandTimeline;

namespace ControlUnitTests
{
    class CommandTimelineTests
    {
        TEST_CLASS(CommandTimelineTests);

        TEST_METHOD(StableIdentityAndSequenceNeverReused);
        TEST_METHOD(PaneIndexesAndViewStateAreIsolated);
        TEST_METHOD(ColdBootstrapScansOnceAndWarmAccessDoesNotRescan);
        TEST_METHOD(LifecycleUpdatesAreIncrementalAndIdempotent);
        TEST_METHOD(OutOfOrderLifecycleStaysUnknown);
        TEST_METHOD(TrustedCompletionMapsResults);
        TEST_METHOD(CapabilityRequiresCompleteNativeLifecycle);
        TEST_METHOD(InvalidationAndEvictionPruneGhostEntries);
        TEST_METHOD(ReflowPreservesIdentityWithoutDuplicates);
        TEST_METHOD(CommandTextCacheIsBoundedAndOutputIsAbsent);
        TEST_METHOD(CloseClearsOwnedStateAndRejectsCallbacks);
        TEST_METHOD(NativeOscLifecycleFeedsPaneTimeline);
        TEST_METHOD(NavigationEmptyOpenClosePreservesNoFakeEntry);
        TEST_METHOD(NavigationInitialOpenAndRestoreSelection);
        TEST_METHOD(NavigationMovesOneEntryAndDoesNotWrap);
        TEST_METHOD(NavigationPageEdgesMoveOneInOneOut);
        TEST_METHOD(NavigationPageFirstAndLastStayWithinViewport);
        TEST_METHOD(NavigationHoverAndClickOnlySelectVisibleEntry);
        TEST_METHOD(NavigationWheelAccumulatesReversesAndSettles);
        TEST_METHOD(NavigationReconcilesRemovalReflowAndNewCommands);
        TEST_METHOD(NavigationStateIsPaneLocalAndRepeatedCloseIsSafe);
        TEST_METHOD(NavigationVisibleProjectionIsBoundedAndNeutralizesLimitedResults);
        TEST_METHOD(ActionLoadPreparesSelectedCommandWithoutExecuting);
        TEST_METHOD(ActionLoadRejectsMissingCommandText);
        TEST_METHOD(ActionLoadRefusesMultilineWithoutBracketedPaste);
        TEST_METHOD(ActionLoadConfirmsLargeCommandBeforeReady);
        TEST_METHOD(ActionLoadTracksExecutionGeneration);
        TEST_METHOD(ActionLateCompletionIsDetectedAfterExecutionStart);
        TEST_METHOD(ActionLoadedInputIsReleasedOnEviction);
        TEST_METHOD(ActionCopyResolvesStableCommandIdNotRowIndex);
        TEST_METHOD(ActionOutputRequiresLiveNativeRangeAndIsNeverCached);
        TEST_METHOD(SearchEmptyQueryProjectsEveryCommand);
        TEST_METHOD(SearchMatchesLiterallyAndCaseInsensitively);
        TEST_METHOD(SearchQueryTruncationIsSurrogateSafe);
        TEST_METHOD(SearchKeepsStableSelectionAcrossQueryChanges);
        TEST_METHOD(SearchSelectsNearestSurvivingMatch);
        TEST_METHOD(SearchNavigationAndWheelWalkFilteredProjection);
        TEST_METHOD(SearchNewCommandFollowsLatestOnlyWhenMatching);
        TEST_METHOD(ShellDegradationStatesAreDistinct);
        TEST_METHOD(HistoryLimitEvictsOldestFirstAndNeverResurrects);
        TEST_METHOD(HistoryLimitClampsToSupportedRange);
        TEST_METHOD(SearchCloseReleasesQueryAndProjection);
        TEST_METHOD(SearchStressAtMaximumHistoryLimit);

        TEST_CLASS_SETUP(ModuleSetup)
        {
            winrt::init_apartment(winrt::apartment_type::single_threaded);
            return true;
        }

        TEST_CLASS_CLEANUP(ModuleCleanup)
        {
            winrt::uninit_apartment();
            return true;
        }

    private:
        static constexpr winrt::guid PaneOne{ 0x10000001, 0x1001, 0x1001, { 0x80, 0x01, 0x10, 0x01, 0x10, 0x01, 0x10, 0x01 } };
        static constexpr winrt::guid PaneTwo{ 0x20000002, 0x2002, 0x2002, { 0x80, 0x02, 0x20, 0x02, 0x20, 0x02, 0x20, 0x02 } };

        static LifecycleUpdate Update(LifecycleEventKind kind,
                                      uint64_t nativeMarkId,
                                      uint64_t nativeRevision,
                                      std::wstring_view commandText = {},
                                      std::optional<uint32_t> exitCode = std::nullopt)
        {
            return {
                .kind = kind,
                .nativeMarkId = nativeMarkId,
                .nativeRevision = nativeRevision,
                .commandText = commandText,
                .trustedExitCode = exitCode,
                .timestamp = std::chrono::system_clock::time_point{ std::chrono::seconds{ nativeRevision } },
            };
        }

        static void Execute(CommandTimelineIndex& index,
                            uint64_t nativeMarkId,
                            uint64_t& nativeRevision,
                            std::wstring_view command,
                            std::optional<uint32_t> exitCode)
        {
            index.ProcessLifecycle(Update(LifecycleEventKind::Prompt, nativeMarkId, ++nativeRevision));
            index.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, nativeMarkId, ++nativeRevision));
            index.ProcessLifecycle(Update(LifecycleEventKind::OutputStart, nativeMarkId, ++nativeRevision, command));
            index.ProcessLifecycle(Update(LifecycleEventKind::CommandFinished, nativeMarkId, ++nativeRevision, command, exitCode));
        }
    };

    void CommandTimelineTests::StableIdentityAndSequenceNeverReused()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t nativeRevision = 0;
        Execute(index, 101, nativeRevision, L"echo same", 0);
        Execute(index, 102, nativeRevision, L"echo same", 0);

        VERIFY_ARE_EQUAL(size_t{ 2 }, index.Entries().size());
        VERIFY_ARE_EQUAL(PaneOne, index.Entries()[0].id.paneSessionId);
        VERIFY_ARE_EQUAL(uint64_t{ 1 }, index.Entries()[0].id.sequence);
        VERIFY_ARE_EQUAL(uint64_t{ 2 }, index.Entries()[1].id.sequence);
        VERIFY_ARE_NOT_EQUAL(index.Entries()[0].id.sequence, index.Entries()[1].id.sequence);

        const std::array<uint64_t, 1> invalid{ 101 };
        index.InvalidateNativeMarks(invalid, ++nativeRevision);
        Execute(index, 103, nativeRevision, L"echo same", 0);
        VERIFY_ARE_EQUAL(uint64_t{ 3 }, index.Entries().back().id.sequence);
        VERIFY_ARE_EQUAL(uint64_t{ 4 }, index.NextSequence());
    }

    void CommandTimelineTests::PaneIndexesAndViewStateAreIsolated()
    {
        CommandTimelineIndex first{ PaneOne };
        CommandTimelineIndex second{ PaneTwo };
        uint64_t firstRevision = 0;
        uint64_t secondRevision = 0;
        Execute(first, 1, firstRevision, L"pwd", 0);
        Execute(second, 1, secondRevision, L"pwd", 0);

        VERIFY_ARE_EQUAL(uint64_t{ 1 }, first.Entries()[0].id.sequence);
        VERIFY_ARE_EQUAL(uint64_t{ 1 }, second.Entries()[0].id.sequence);
        VERIFY_ARE_NOT_EQUAL(first.Entries()[0].id.paneSessionId, second.Entries()[0].id.paneSessionId);

        const auto secondRevisionBefore = second.Revision();
        first.ProcessLifecycle(Update(LifecycleEventKind::Prompt, 2, ++firstRevision));
        VERIFY_ARE_EQUAL(secondRevisionBefore, second.Revision());

        CommandTimelineViewState firstView;
        CommandTimelineViewState secondView;
        firstView.selectedCommandId = first.Entries()[0].id;
        firstView.visibleNativeAnchor = first.Entries()[0].nativeMarkId;
        firstView.selectedVisualSlot = 3;
        firstView.loadedIntoInput = true;
        firstView.executionGeneration = 9;
        VERIFY_IS_FALSE(secondView.selectedCommandId.has_value());
        VERIFY_IS_FALSE(secondView.visibleNativeAnchor.has_value());
        VERIFY_ARE_EQUAL(uint64_t{ 0 }, secondView.executionGeneration);
    }

    void CommandTimelineTests::ColdBootstrapScansOnceAndWarmAccessDoesNotRescan()
    {
        CommandTimelineIndex index{ PaneOne };
        size_t loaderCalls = 0;
        const auto loader = [&]() {
            ++loaderCalls;
            return NativeBootstrapSnapshot{
                .nativeRevision = 7,
                .marks = {
                    { .nativeMarkId = 41, .commandText = L"first", .hasCommand = true, .hasOutput = true, .trustedExitCode = 0 },
                    { .nativeMarkId = 42, .commandText = L"second", .hasCommand = true, .hasOutput = true, .trustedExitCode = 1 },
                },
            };
        };

        const auto& cold = index.Access(7, loader);
        VERIFY_ARE_EQUAL(size_t{ 1 }, loaderCalls);
        VERIFY_ARE_EQUAL(size_t{ 2 }, cold.size());
        VERIFY_ARE_EQUAL(uint64_t{ 1 }, cold[0].id.sequence);
        VERIFY_ARE_EQUAL(uint64_t{ 2 }, cold[1].id.sequence);

        const auto& warm = index.Access(7, loader);
        VERIFY_ARE_EQUAL(size_t{ 1 }, loaderCalls);
        VERIFY_ARE_EQUAL(size_t{ 1 }, index.BootstrapScanCount());
        VERIFY_ARE_EQUAL(size_t{ 2 }, warm.size());
    }

    void CommandTimelineTests::LifecycleUpdatesAreIncrementalAndIdempotent()
    {
        CommandTimelineIndex index{ PaneOne };
        // 133;B alone is the user composing input at the prompt. No command
        // exists yet, so nothing may be listed for it.
        index.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, 11, 1));
        const auto revisionAfterStart = index.Revision();
        index.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, 11, 1));
        VERIFY_IS_TRUE(index.Entries().empty());
        VERIFY_ARE_EQUAL(revisionAfterStart, index.Revision());

        // The entry materializes when the command actually executes, and the
        // earlier 133;B still lets it read as Running rather than Unknown.
        index.ProcessLifecycle(Update(LifecycleEventKind::OutputStart, 11, 2, L"build"));
        VERIFY_ARE_EQUAL(size_t{ 1 }, index.Entries().size());
        VERIFY_ARE_EQUAL(static_cast<int>(CommandLifecycleState::Output), static_cast<int>(index.Entries()[0].lifecycleState));
        VERIFY_ARE_EQUAL(static_cast<int>(ExecutionResult::Running), static_cast<int>(index.Entries()[0].executionResult));
        VERIFY_ARE_NOT_EQUAL(static_cast<int>(ExecutionResult::Succeeded), static_cast<int>(index.Entries()[0].executionResult));

        const auto outputRevision = index.Revision();
        index.ProcessLifecycle(Update(LifecycleEventKind::OutputStart, 11, 2, L"build"));
        VERIFY_ARE_EQUAL(outputRevision, index.Revision());
    }

    void CommandTimelineTests::OutOfOrderLifecycleStaysUnknown()
    {
        CommandTimelineIndex index{ PaneOne };
        index.ProcessLifecycle(Update(LifecycleEventKind::OutputStart, 21, 1, L"orphan"));
        VERIFY_ARE_EQUAL(size_t{ 1 }, index.Entries().size());
        VERIFY_ARE_EQUAL(static_cast<int>(ExecutionResult::Unknown), static_cast<int>(index.Entries()[0].executionResult));

        index.ProcessLifecycle(Update(LifecycleEventKind::CommandFinished, 21, 2, L"orphan", 0));
        VERIFY_ARE_EQUAL(static_cast<int>(ExecutionResult::Unknown), static_cast<int>(index.Entries()[0].executionResult));
        VERIFY_IS_FALSE(index.Entries()[0].trustedExitCode.has_value());
        VERIFY_ARE_EQUAL(static_cast<int>(CommandLifecycleState::Incomplete), static_cast<int>(index.Entries()[0].lifecycleState));

        index.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, 22, 3));
        index.ProcessLifecycle(Update(LifecycleEventKind::Prompt, 23, 4));
        const auto& incomplete = index.Entries().back();
        VERIFY_ARE_EQUAL(static_cast<int>(CommandLifecycleState::Incomplete), static_cast<int>(incomplete.lifecycleState));
        VERIFY_ARE_EQUAL(static_cast<int>(ExecutionResult::Unknown), static_cast<int>(incomplete.executionResult));
        VERIFY_IS_NULL(index.Current());
    }

    void CommandTimelineTests::TrustedCompletionMapsResults()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t nativeRevision = 0;
        Execute(index, 31, nativeRevision, L"true", 0);
        Execute(index, 32, nativeRevision, L"false", 17);
        VERIFY_ARE_EQUAL(static_cast<int>(ExecutionResult::Succeeded), static_cast<int>(index.Entries()[0].executionResult));
        VERIFY_ARE_EQUAL(uint32_t{ 0 }, *index.Entries()[0].trustedExitCode);
        VERIFY_ARE_EQUAL(static_cast<int>(ExecutionResult::Failed), static_cast<int>(index.Entries()[1].executionResult));
        VERIFY_ARE_EQUAL(uint32_t{ 17 }, *index.Entries()[1].trustedExitCode);

        index.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, 33, ++nativeRevision));
        index.ProcessLifecycle(Update(LifecycleEventKind::CommandCancelled, 33, ++nativeRevision));
        VERIFY_ARE_EQUAL(static_cast<int>(ExecutionResult::Cancelled), static_cast<int>(index.Entries()[2].executionResult));
    }

    void CommandTimelineTests::CapabilityRequiresCompleteNativeLifecycle()
    {
        VERIFY_ARE_EQUAL(static_cast<int>(ShellIntegrationCapability::Limited),
                         static_cast<int>(InferCapabilityFallbackFromCommandline(L"C:\\Windows\\System32\\cmd.exe /d")));
        VERIFY_ARE_EQUAL(static_cast<int>(ShellIntegrationCapability::Unknown),
                         static_cast<int>(InferCapabilityFallbackFromCommandline(L"pwsh.exe")));

        CommandTimelineIndex index{ PaneOne };
        index.SetCapabilityFallback(ShellIntegrationCapability::Limited);
        index.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, 51, 1));
        index.ProcessLifecycle(Update(LifecycleEventKind::OutputStart, 51, 2, L"echo error failed red"));
        index.ProcessLifecycle(Update(LifecycleEventKind::Prompt, 51, 3));
        index.ProcessLifecycle(Update(LifecycleEventKind::CommandFinished, 51, 4, L"echo error failed red", 0));
        VERIFY_ARE_EQUAL(static_cast<int>(ShellIntegrationCapability::Limited), static_cast<int>(index.Capability()));
        VERIFY_ARE_EQUAL(static_cast<int>(ExecutionResult::Unknown), static_cast<int>(index.Entries()[0].executionResult));

        index.ProcessLifecycle(Update(LifecycleEventKind::Prompt, 52, 5));
        index.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, 52, 6));
        index.ProcessLifecycle(Update(LifecycleEventKind::OutputStart, 52, 7, L"echo reliable"));
        VERIFY_ARE_EQUAL(static_cast<int>(ShellIntegrationCapability::Limited), static_cast<int>(index.Capability()));
        index.ProcessLifecycle(Update(LifecycleEventKind::CommandFinished, 52, 8, L"echo reliable", 0));
        VERIFY_ARE_EQUAL(static_cast<int>(ShellIntegrationCapability::Full), static_cast<int>(index.Capability()));
        VERIFY_ARE_EQUAL(static_cast<int>(ShellIntegrationCapability::Full), static_cast<int>(index.Entries()[0].shellIntegrationCapability));
    }

    void CommandTimelineTests::InvalidationAndEvictionPruneGhostEntries()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t nativeRevision = 0;
        Execute(index, 61, nativeRevision, L"old", 0);
        Execute(index, 62, nativeRevision, L"new", 0);

        const std::array<uint64_t, 1> clearScrollback{ 61 };
        index.InvalidateNativeMarks(clearScrollback, ++nativeRevision);
        VERIFY_ARE_EQUAL(size_t{ 1 }, index.Entries().size());
        VERIFY_ARE_EQUAL(uint64_t{ 62 }, index.Entries()[0].nativeMarkId);

        const std::array<uint64_t, 1> eviction{ 62 };
        index.InvalidateNativeMarks(eviction, ++nativeRevision);
        VERIFY_IS_TRUE(index.Entries().empty());
        VERIFY_ARE_EQUAL(uint64_t{ 3 }, index.NextSequence());
    }

    void CommandTimelineTests::ReflowPreservesIdentityWithoutDuplicates()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t nativeRevision = 0;
        Execute(index, 71, nativeRevision, L"one", 0);
        Execute(index, 72, nativeRevision, L"two", 0);
        const auto firstId = index.Entries()[0].id;
        const auto secondId = index.Entries()[1].id;

        const std::array<uint64_t, 2> allSurvive{ 71, 72 };
        index.ReconcileReflow(allSurvive, nativeRevision);
        index.ReconcileReflow(allSurvive, nativeRevision);
        VERIFY_ARE_EQUAL(size_t{ 2 }, index.Entries().size());
        VERIFY_IS_TRUE(index.Find(firstId) != nullptr);
        VERIFY_IS_TRUE(index.Find(secondId) != nullptr);

        const std::array<uint64_t, 1> oneSurvives{ 72 };
        index.ReconcileReflow(oneSurvives, ++nativeRevision);
        VERIFY_ARE_EQUAL(size_t{ 1 }, index.Entries().size());
        VERIFY_ARE_EQUAL(secondId.sequence, index.Entries()[0].id.sequence);
    }

    void CommandTimelineTests::CommandTextCacheIsBoundedAndOutputIsAbsent()
    {
        CommandTimelineIndex index{ PaneOne, { .maxCommandTextLength = 5 } };
        index.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, 81, 1));
        index.ProcessLifecycle(Update(LifecycleEventKind::OutputStart, 81, 2, L"123456789"));
        VERIFY_ARE_EQUAL(std::wstring{ L"12345" }, index.Entries()[0].cachedCommandText);
        VERIFY_ARE_EQUAL(size_t{ 5 }, index.CachedCommandTextCharacters());

        CommandTimelineIndex surrogateSafe{ PaneOne, { .maxCommandTextLength = 2 } };
        const std::wstring text{ L'A', wchar_t{ 0xD83D }, wchar_t{ 0xDE80 }, L'B' };
        surrogateSafe.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, 82, 1));
        surrogateSafe.ProcessLifecycle(Update(LifecycleEventKind::OutputStart, 82, 2, text));
        VERIFY_ARE_EQUAL(std::wstring{ L"A" }, surrogateSafe.Entries()[0].cachedCommandText);
    }

    void CommandTimelineTests::CloseClearsOwnedStateAndRejectsCallbacks()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t nativeRevision = 0;
        Execute(index, 91, nativeRevision, L"private command", 0);
        const auto nextSequence = index.NextSequence();
        index.Close();
        index.Close();
        index.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, 92, ++nativeRevision));

        VERIFY_IS_TRUE(index.IsClosed());
        VERIFY_IS_TRUE(index.Entries().empty());
        VERIFY_ARE_EQUAL(size_t{ 0 }, index.CachedCommandTextCharacters());
        VERIFY_IS_NULL(index.Current());
        VERIFY_ARE_EQUAL(nextSequence, index.NextSequence());
    }

    void CommandTimelineTests::NativeOscLifecycleFeedsPaneTimeline()
    {
        auto settings = winrt::make_self<MockControlSettings>();
        auto connection = winrt::make_self<MockConnection>();
        auto core = winrt::make_self<winrt::Microsoft::Terminal::Control::implementation::ControlCore>(*settings, *settings, *connection);
        // Keep resize notifications synchronous in TAEF, matching the other
        // ControlCore unit tests and avoiding a dependency on its UI dispatcher.
        core->_inUnitTests = true;
        auto cleanup = wil::scope_exit([&]() noexcept {
            try
            {
                if (core)
                {
                    core->Close();
                }
            }
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
            core = nullptr;
            connection = nullptr;
            settings = nullptr;
        });
        VERIFY_IS_TRUE(core->Initialize(270, 380, 1.0));

        connection->WriteInput(winrt_wstring_to_array_view(L"\x1b]133;A\aPS> \x1b]133;B\aecho same\x1b]133;C\a\r\nSECRET_OUTPUT\r\n\x1b]133;D;0\a"));
        auto first = core->CommandTimelineSnapshot();
        VERIFY_ARE_EQUAL(size_t{ 1 }, first.entries.size());
        VERIFY_ARE_EQUAL(std::wstring{ L"echo same" }, first.entries[0].cachedCommandText);
        VERIFY_IS_TRUE(first.entries[0].cachedCommandText.find(L"SECRET_OUTPUT") == std::wstring::npos);
        VERIFY_ARE_EQUAL(static_cast<int>(ExecutionResult::Succeeded), static_cast<int>(first.entries[0].executionResult));
        VERIFY_ARE_EQUAL(static_cast<int>(ShellIntegrationCapability::Full), static_cast<int>(first.capability));
        VERIFY_ARE_EQUAL(size_t{ 1 }, first.bootstrapScanCount);

        connection->WriteInput(winrt_wstring_to_array_view(L"\r\n\x1b]133;A\aPS> \x1b]133;B\aecho same\x1b]133;C\a\r\nSECOND_OUTPUT\r\n\x1b]133;D;1\a"));
        auto second = core->CommandTimelineSnapshot();
        VERIFY_ARE_EQUAL(size_t{ 2 }, second.entries.size());
        VERIFY_ARE_NOT_EQUAL(second.entries[0].id.sequence, second.entries[1].id.sequence);
        VERIFY_ARE_EQUAL(size_t{ 1 }, second.bootstrapScanCount);
        const auto preservedId = second.entries[0].id;

        core->SizeChanged(180, 380);
        const auto resized = core->CommandTimelineSnapshot();
        VERIFY_ARE_EQUAL(size_t{ 1 }, resized.bootstrapScanCount);
        VERIFY_IS_TRUE(std::any_of(resized.entries.begin(), resized.entries.end(), [&](const auto& entry) {
            return entry.id == preservedId;
        }));

        const auto opened = core->OpenCommandTimeline(2);
        VERIFY_ARE_EQUAL(size_t{ 2 }, opened.visibleEntries.size());
        core->CloseCommandTimelineOverlay();
        const auto reopened = core->OpenCommandTimeline(2);
        VERIFY_ARE_EQUAL(size_t{ 2 }, reopened.visibleEntries.size());
        VERIFY_ARE_EQUAL(size_t{ 1 }, core->CommandTimelineSnapshot().bootstrapScanCount);
        core->CloseCommandTimelineOverlay();

        CommandTimelineViewState viewState;
        viewState.selectedCommandId = resized.entries.back().id;
        viewState.visibleNativeAnchor = resized.entries.back().nativeMarkId;
        viewState.selectedVisualSlot = 1;
        viewState.loadedIntoInput = true;
        viewState.executionGeneration = 3;
        core->CommandTimelineViewState(viewState);
        VERIFY_IS_TRUE(core->CommandTimelineSnapshot().viewState.selectedCommandId.has_value());

        core->Close();
        connection->WriteInput(winrt_wstring_to_array_view(L"\x1b]133;A\aignored"));
        const auto closed = core->CommandTimelineSnapshot();
        VERIFY_IS_TRUE(closed.closed);
        VERIFY_IS_TRUE(closed.entries.empty());
        VERIFY_IS_FALSE(closed.viewState.selectedCommandId.has_value());
        VERIFY_IS_FALSE(closed.viewState.visibleNativeAnchor.has_value());
        VERIFY_ARE_EQUAL(uint64_t{ 0 }, closed.viewState.executionGeneration);

        // Release renderer, terminal, connection revokers, and WinRT objects
        // before TAEF advances to the next class or unloads this test module.
        core = nullptr;
        connection = nullptr;
        settings = nullptr;
        cleanup.release();
    }

    void CommandTimelineTests::NavigationEmptyOpenClosePreservesNoFakeEntry()
    {
        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        const std::vector<CommandTimelineEntry> entries;

        const auto opened = navigation.Open(entries, viewState, ShellIntegrationCapability::Unknown, 0);
        VERIFY_IS_TRUE(opened.open);
        VERIFY_IS_TRUE(opened.visibleEntries.empty());
        VERIFY_ARE_EQUAL(size_t{ 1 }, navigation.VisibleCapacity());
        VERIFY_IS_FALSE(viewState.selectedCommandId.has_value());

        navigation.Close();
        navigation.Close();
        VERIFY_IS_FALSE(navigation.IsOpen());
        VERIFY_IS_FALSE(navigation.WheelSettlePending());
    }

    void CommandTimelineTests::NavigationInitialOpenAndRestoreSelection()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        for (uint64_t nativeMark = 1; nativeMark <= 5; ++nativeMark)
        {
            Execute(index, nativeMark, revision, std::to_wstring(nativeMark), 0);
        }

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        auto latest = navigation.Open(index.Entries(), viewState, index.Capability(), 3);
        VERIFY_ARE_EQUAL(uint64_t{ 5 }, viewState.selectedCommandId->sequence);
        VERIFY_ARE_EQUAL(size_t{ 2 }, *viewState.selectedVisualSlot);
        VERIFY_ARE_EQUAL(size_t{ 3 }, latest.visibleEntries.size());

        navigation.Close();
        viewState.selectedCommandId = index.Entries()[3].id;
        viewState.visibleNativeAnchor = index.Entries()[2].nativeMarkId;
        viewState.selectedVisualSlot = 1;
        const auto restored = navigation.Open(index.Entries(), viewState, index.Capability(), 3);
        VERIFY_ARE_EQUAL(uint64_t{ 4 }, viewState.selectedCommandId->sequence);
        VERIFY_ARE_EQUAL(uint64_t{ 3 }, restored.visibleEntries.front().id.sequence);
        VERIFY_ARE_EQUAL(size_t{ 1 }, restored.selectedVisualSlot);
    }

    void CommandTimelineTests::NavigationMovesOneEntryAndDoesNotWrap()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        for (uint64_t nativeMark = 1; nativeMark <= 4; ++nativeMark)
        {
            Execute(index, nativeMark, revision, L"command", 0);
        }

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 2);
        navigation.Navigate(NavigationAction::Previous, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 3 }, viewState.selectedCommandId->sequence);
        navigation.Navigate(NavigationAction::Previous, index.Entries(), viewState, index.Capability());
        navigation.Navigate(NavigationAction::Previous, index.Entries(), viewState, index.Capability());
        navigation.Navigate(NavigationAction::Previous, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 1 }, viewState.selectedCommandId->sequence);

        for (size_t step = 0; step < 5; ++step)
        {
            navigation.Navigate(NavigationAction::Next, index.Entries(), viewState, index.Capability());
        }
        VERIFY_ARE_EQUAL(uint64_t{ 4 }, viewState.selectedCommandId->sequence);
    }

    void CommandTimelineTests::NavigationPageEdgesMoveOneInOneOut()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        for (uint64_t nativeMark = 1; nativeMark <= 6; ++nativeMark)
        {
            Execute(index, nativeMark, revision, L"command", 0);
        }

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        viewState.selectedCommandId = index.Entries()[2].id;
        viewState.visibleNativeAnchor = index.Entries()[2].nativeMarkId;
        viewState.selectedVisualSlot = 0;
        const auto beforeUp = navigation.Open(index.Entries(), viewState, index.Capability(), 3);
        VERIFY_ARE_EQUAL(uint64_t{ 3 }, beforeUp.visibleEntries.front().id.sequence);
        const auto afterUp = navigation.Navigate(NavigationAction::Previous, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(size_t{ 3 }, afterUp.visibleEntries.size());
        VERIFY_ARE_EQUAL(uint64_t{ 2 }, afterUp.visibleEntries.front().id.sequence);
        VERIFY_ARE_EQUAL(uint64_t{ 4 }, afterUp.visibleEntries.back().id.sequence);

        viewState.selectedCommandId = index.Entries()[3].id;
        viewState.visibleNativeAnchor = index.Entries()[1].nativeMarkId;
        viewState.selectedVisualSlot = 2;
        navigation.Close();
        navigation.Open(index.Entries(), viewState, index.Capability(), 3);
        const auto afterDown = navigation.Navigate(NavigationAction::Next, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(size_t{ 3 }, afterDown.visibleEntries.size());
        VERIFY_ARE_EQUAL(uint64_t{ 3 }, afterDown.visibleEntries.front().id.sequence);
        VERIFY_ARE_EQUAL(uint64_t{ 5 }, afterDown.visibleEntries.back().id.sequence);
    }

    void CommandTimelineTests::NavigationPageFirstAndLastStayWithinViewport()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        for (uint64_t nativeMark = 1; nativeMark <= 5; ++nativeMark)
        {
            Execute(index, nativeMark, revision, L"command", 0);
        }

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 3);
        navigation.Navigate(NavigationAction::PageFirst, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 3 }, viewState.selectedCommandId->sequence);
        navigation.Navigate(NavigationAction::PageLast, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 5 }, viewState.selectedCommandId->sequence);
    }

    void CommandTimelineTests::NavigationHoverAndClickOnlySelectVisibleEntry()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"first", 0);
        Execute(index, 2, revision, L"second", 0);
        Execute(index, 3, revision, L"third", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 3);
        const auto hovered = navigation.SelectVisibleEntry(0, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 1 }, viewState.selectedCommandId->sequence);
        VERIFY_IS_TRUE(hovered.visibleEntries[0].selected);
        VERIFY_IS_FALSE(viewState.loadedIntoInput);
        VERIFY_ARE_EQUAL(uint64_t{ 0 }, viewState.executionGeneration);

        navigation.SelectVisibleEntry(2, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 3 }, viewState.selectedCommandId->sequence);
        VERIFY_IS_FALSE(viewState.loadedIntoInput);
    }

    void CommandTimelineTests::NavigationWheelAccumulatesReversesAndSettles()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        for (uint64_t nativeMark = 1; nativeMark <= 5; ++nativeMark)
        {
            Execute(index, nativeMark, revision, L"command", 0);
        }

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 3);
        navigation.ApplyWheelDelta(60, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 5 }, viewState.selectedCommandId->sequence);
        VERIFY_ARE_EQUAL(60, navigation.WheelDeltaRemainder());

        navigation.ApplyWheelDelta(-30, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(30, navigation.WheelDeltaRemainder());
        navigation.ApplyWheelDelta(90, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 4 }, viewState.selectedCommandId->sequence);
        VERIFY_ARE_EQUAL(0, navigation.WheelDeltaRemainder());
        navigation.ApplyWheelDelta(-1, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(-1, navigation.WheelDeltaRemainder());
        VERIFY_IS_TRUE(navigation.WheelSettlePending());
        navigation.SettleWheel();
        VERIFY_ARE_EQUAL(0, navigation.WheelDeltaRemainder());
        VERIFY_IS_FALSE(navigation.WheelSettlePending());

        navigation.ApplyWheelDelta(360, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 1 }, viewState.selectedCommandId->sequence);
    }

    void CommandTimelineTests::NavigationReconcilesRemovalReflowAndNewCommands()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        for (uint64_t nativeMark = 1; nativeMark <= 4; ++nativeMark)
        {
            Execute(index, nativeMark, revision, L"command", 0);
        }

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 2);
        navigation.Navigate(NavigationAction::Previous, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 3 }, viewState.selectedCommandId->sequence);

        Execute(index, 5, revision, L"new", 0);
        navigation.Reconcile(index.Entries(), viewState, index.Capability(), 2);
        VERIFY_ARE_EQUAL(uint64_t{ 3 }, viewState.selectedCommandId->sequence);

        const std::array<uint64_t, 1> removeSelected{ 3 };
        index.InvalidateNativeMarks(removeSelected, ++revision);
        navigation.Reconcile(index.Entries(), viewState, index.Capability(), 2);
        VERIFY_ARE_EQUAL(uint64_t{ 2 }, viewState.selectedCommandId->sequence);

        viewState.selectedCommandId = index.Entries().back().id;
        navigation.Reconcile(index.Entries(), viewState, index.Capability(), 2);
        Execute(index, 6, revision, L"latest", 0);
        navigation.Reconcile(index.Entries(), viewState, index.Capability(), 2);
        VERIFY_ARE_EQUAL(uint64_t{ 6 }, viewState.selectedCommandId->sequence);

        const auto stableId = viewState.selectedCommandId;
        const std::array<uint64_t, 4> allSurvive{ 1, 2, 4, 5 };
        index.ReconcileReflow(allSurvive, ++revision);
        navigation.Reconcile(index.Entries(), viewState, index.Capability(), 2);
        VERIFY_IS_TRUE(viewState.selectedCommandId.has_value());
        VERIFY_ARE_NOT_EQUAL(stableId->sequence, viewState.selectedCommandId->sequence);
    }

    void CommandTimelineTests::NavigationStateIsPaneLocalAndRepeatedCloseIsSafe()
    {
        CommandTimelineIndex first{ PaneOne };
        CommandTimelineIndex second{ PaneTwo };
        uint64_t firstRevision = 0;
        uint64_t secondRevision = 0;
        Execute(first, 1, firstRevision, L"same", 0);
        Execute(first, 2, firstRevision, L"same", 0);
        Execute(second, 1, secondRevision, L"same", 0);
        Execute(second, 2, secondRevision, L"same", 0);

        CommandTimelineNavigationModel firstNavigation;
        CommandTimelineNavigationModel secondNavigation;
        CommandTimelineViewState firstView;
        CommandTimelineViewState secondView;
        firstNavigation.Open(first.Entries(), firstView, first.Capability(), 1);
        secondNavigation.Open(second.Entries(), secondView, second.Capability(), 1);
        firstNavigation.ApplyWheelDelta(60, first.Entries(), firstView, first.Capability());
        VERIFY_ARE_EQUAL(60, firstNavigation.WheelDeltaRemainder());
        VERIFY_ARE_EQUAL(0, secondNavigation.WheelDeltaRemainder());

        const auto preserved = firstView.selectedCommandId;
        firstNavigation.Close();
        firstNavigation.Close();
        VERIFY_IS_TRUE(firstView.selectedCommandId == preserved);
        VERIFY_IS_TRUE(secondNavigation.IsOpen());
    }

    void CommandTimelineTests::NavigationVisibleProjectionIsBoundedAndNeutralizesLimitedResults()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        for (uint64_t nativeMark = 1; nativeMark <= 10; ++nativeMark)
        {
            Execute(index, nativeMark, revision, L"same command", 0);
        }

        auto entries = index.Entries();
        entries.back().shellIntegrationCapability = ShellIntegrationCapability::Limited;
        entries.back().executionResult = ExecutionResult::Succeeded;
        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        const auto presentation = navigation.Open(entries, viewState, ShellIntegrationCapability::Limited, 2);
        VERIFY_ARE_EQUAL(size_t{ 2 }, presentation.visibleEntries.size());
        VERIFY_ARE_EQUAL(std::wstring{ L"same command" }, presentation.visibleEntries[0].commandText);
        VERIFY_ARE_EQUAL(std::wstring{ L"same command" }, presentation.visibleEntries[1].commandText);
        VERIFY_ARE_NOT_EQUAL(presentation.visibleEntries[0].id.sequence, presentation.visibleEntries[1].id.sequence);
        VERIFY_ARE_EQUAL(static_cast<int>(ExecutionResult::Unknown),
                         static_cast<int>(presentation.visibleEntries[1].executionResult));
        VERIFY_ARE_EQUAL(uint64_t{ 10 }, presentation.visibleEntries[1].id.sequence);
    }

    void CommandTimelineTests::ActionLoadPreparesSelectedCommandWithoutExecuting()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"git status", 0);
        Execute(index, 2, revision, L"cargo build", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 4);
        navigation.Navigate(NavigationAction::Previous, index.Entries(), viewState, index.Capability());

        CommandTimelineActionModel actions;
        const auto request = actions.Prepare(CommandActionKind::LoadIntoInput,
                                             index.Entries(),
                                             viewState,
                                             CommandLoadPolicy{});

        VERIFY_ARE_EQUAL(static_cast<int>(CommandActionStatus::Ready), static_cast<int>(request.status));
        VERIFY_IS_TRUE(request.Actionable());
        VERIFY_ARE_EQUAL(std::wstring{ L"git status" }, request.commandText);
        VERIFY_ARE_EQUAL(uint64_t{ 1 }, request.id.sequence);
        // The prepared payload must never carry a submission character.
        VERIFY_ARE_EQUAL(std::wstring::npos, request.commandText.find_first_of(L"\r\n"));
        VERIFY_IS_FALSE(request.multiline);
    }

    void CommandTimelineTests::ActionLoadRejectsMissingCommandText()
    {
        // An executed command whose text could not be captured still lists,
        // but its text-dependent actions must refuse.
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        index.ProcessLifecycle(Update(LifecycleEventKind::Prompt, 1, ++revision));
        index.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, 1, ++revision));
        index.ProcessLifecycle(Update(LifecycleEventKind::OutputStart, 1, ++revision, L""));
        VERIFY_ARE_EQUAL(size_t{ 1 }, index.Entries().size());

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 4);

        CommandTimelineActionModel actions;
        const auto load = actions.Prepare(CommandActionKind::LoadIntoInput, index.Entries(), viewState, CommandLoadPolicy{});
        VERIFY_ARE_EQUAL(static_cast<int>(CommandActionStatus::CommandTextUnavailable), static_cast<int>(load.status));
        VERIFY_IS_FALSE(load.Actionable());

        const auto copy = actions.Prepare(CommandActionKind::CopyCommand, index.Entries(), viewState, CommandLoadPolicy{});
        VERIFY_ARE_EQUAL(static_cast<int>(CommandActionStatus::CommandTextUnavailable), static_cast<int>(copy.status));

        // An empty timeline cannot produce an actionable request at all.
        CommandTimelineViewState emptyView;
        const auto none = actions.Prepare(CommandActionKind::LoadIntoInput, {}, emptyView, CommandLoadPolicy{});
        VERIFY_ARE_EQUAL(static_cast<int>(CommandActionStatus::NoSelection), static_cast<int>(none.status));
    }

    void CommandTimelineTests::ActionLoadRefusesMultilineWithoutBracketedPaste()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"for i in 1 2 3\ndo echo $i\ndone", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 4);

        CommandTimelineActionModel actions;
        VERIFY_IS_TRUE(CommandTimelineActionModel::IsMultiline(L"a\nb"));
        VERIFY_IS_FALSE(CommandTimelineActionModel::IsMultiline(L"a b"));

        const CommandLoadPolicy unbracketed{ .bracketedPasteEnabled = false };
        const auto refused = actions.Prepare(CommandActionKind::LoadIntoInput, index.Entries(), viewState, unbracketed);
        VERIFY_ARE_EQUAL(static_cast<int>(CommandActionStatus::MultilineUnsafe), static_cast<int>(refused.status));
        VERIFY_IS_FALSE(refused.Actionable());
        // A refused request must not become actionable by confirming it.
        VERIFY_IS_FALSE(CommandTimelineActionModel::Confirm(refused).Actionable());

        const CommandLoadPolicy bracketed{ .bracketedPasteEnabled = true, .warnOnMultilineLoad = false };
        const auto allowed = actions.Prepare(CommandActionKind::LoadIntoInput, index.Entries(), viewState, bracketed);
        VERIFY_ARE_EQUAL(static_cast<int>(CommandActionStatus::Ready), static_cast<int>(allowed.status));
        VERIFY_IS_TRUE(allowed.multiline);
    }

    void CommandTimelineTests::ActionLoadConfirmsLargeCommandBeforeReady()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, std::wstring(2000, L'x'), 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 4);

        CommandTimelineActionModel actions;
        const CommandLoadPolicy policy{ .largeLoadCharacterThreshold = 1024, .bracketedPasteEnabled = true };
        const auto pending = actions.Prepare(CommandActionKind::LoadIntoInput, index.Entries(), viewState, policy);
        VERIFY_ARE_EQUAL(static_cast<int>(CommandActionStatus::ConfirmationRequired), static_cast<int>(pending.status));
        VERIFY_IS_FALSE(pending.Actionable());
        VERIFY_ARE_EQUAL(size_t{ 2000 }, pending.characterCount);

        const auto confirmed = CommandTimelineActionModel::Confirm(pending);
        VERIFY_IS_TRUE(confirmed.Actionable());
        VERIFY_ARE_EQUAL(confirmed.id, pending.id);

        // Below the threshold no confirmation is asked for.
        const CommandLoadPolicy generous{ .largeLoadCharacterThreshold = 4096, .bracketedPasteEnabled = true };
        VERIFY_IS_TRUE(actions.Prepare(CommandActionKind::LoadIntoInput, index.Entries(), viewState, generous).Actionable());
    }

    void CommandTimelineTests::ActionLoadTracksExecutionGeneration()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"echo one", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 4);

        CommandTimelineActionModel actions;
        VERIFY_IS_FALSE(viewState.loadedIntoInput);
        VERIFY_ARE_EQUAL(uint64_t{ 0 }, viewState.executionGeneration);

        const auto request = actions.Prepare(CommandActionKind::LoadIntoInput, index.Entries(), viewState, CommandLoadPolicy{});
        actions.NotifyLoaded(viewState, request);

        VERIFY_IS_TRUE(viewState.loadedIntoInput);
        VERIFY_IS_TRUE(viewState.loadedCommandId.has_value());
        VERIFY_ARE_EQUAL(request.id, *viewState.loadedCommandId);
        VERIFY_ARE_EQUAL(uint64_t{ 1 }, viewState.executionGeneration);

        // A non-load request must never mark the input as loaded.
        CommandTimelineViewState other = viewState;
        const auto copy = actions.Prepare(CommandActionKind::CopyCommand, index.Entries(), viewState, CommandLoadPolicy{});
        actions.NotifyLoaded(other, copy);
        VERIFY_ARE_EQUAL(viewState.executionGeneration, other.executionGeneration);
    }

    void CommandTimelineTests::ActionLateCompletionIsDetectedAfterExecutionStart()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"sleep 30", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 4);

        CommandTimelineActionModel actions;
        const auto request = actions.Prepare(CommandActionKind::LoadIntoInput, index.Entries(), viewState, CommandLoadPolicy{});
        actions.NotifyLoaded(viewState, request);

        const auto inFlightGeneration = viewState.executionGeneration;
        VERIFY_IS_TRUE(CommandTimelineActionModel::IsCurrentGeneration(viewState, inFlightGeneration));

        // The pane starts running a command; the earlier generation is retired.
        actions.NotifyExecutionStarted(viewState);
        VERIFY_IS_FALSE(CommandTimelineActionModel::IsCurrentGeneration(viewState, inFlightGeneration));
        VERIFY_IS_TRUE(CommandTimelineActionModel::IsCurrentGeneration(viewState, viewState.executionGeneration));
        VERIFY_IS_FALSE(viewState.loadedIntoInput);
        VERIFY_IS_FALSE(viewState.loadedCommandId.has_value());
    }

    void CommandTimelineTests::ActionLoadedInputIsReleasedOnEviction()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"first", 0);
        Execute(index, 2, revision, L"second", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 4);
        navigation.Navigate(NavigationAction::Previous, index.Entries(), viewState, index.Capability());

        CommandTimelineActionModel actions;
        const auto request = actions.Prepare(CommandActionKind::LoadIntoInput, index.Entries(), viewState, CommandLoadPolicy{});
        actions.NotifyLoaded(viewState, request);
        VERIFY_IS_TRUE(viewState.loadedIntoInput);

        // Still present after an unrelated reconcile.
        actions.ReconcileLoadedInput(index.Entries(), viewState);
        VERIFY_IS_TRUE(viewState.loadedIntoInput);

        const std::array<uint64_t, 1> evicted{ 1 };
        index.InvalidateNativeMarks(evicted, ++revision);
        actions.ReconcileLoadedInput(index.Entries(), viewState);
        VERIFY_IS_FALSE(viewState.loadedIntoInput);
        VERIFY_IS_FALSE(viewState.loadedCommandId.has_value());

        // A request prepared against the evicted command is no longer actionable.
        navigation.Reconcile(index.Entries(), viewState, index.Capability(), 4);
        const auto after = actions.Prepare(CommandActionKind::LoadIntoInput, index.Entries(), viewState, CommandLoadPolicy{});
        VERIFY_ARE_NOT_EQUAL(request.id.sequence, after.id.sequence);
    }

    void CommandTimelineTests::ActionCopyResolvesStableCommandIdNotRowIndex()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"alpha", 0);
        Execute(index, 2, revision, L"beta", 0);
        Execute(index, 3, revision, L"gamma", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 3);
        navigation.SelectVisibleEntry(1, index.Entries(), viewState, index.Capability());

        CommandTimelineActionModel actions;
        const auto before = actions.Prepare(CommandActionKind::CopyCommand, index.Entries(), viewState, CommandLoadPolicy{});
        VERIFY_ARE_EQUAL(std::wstring{ L"beta" }, before.commandText);
        VERIFY_ARE_EQUAL(uint64_t{ 2 }, before.id.sequence);

        // Dropping the first entry shifts every row index by one. The selection
        // must still resolve to the same command, not to whatever now sits in
        // the old row.
        const std::array<uint64_t, 1> evicted{ 1 };
        index.InvalidateNativeMarks(evicted, ++revision);
        navigation.Reconcile(index.Entries(), viewState, index.Capability(), 3);

        const auto after = actions.Prepare(CommandActionKind::CopyCommand, index.Entries(), viewState, CommandLoadPolicy{});
        VERIFY_ARE_EQUAL(before.id, after.id);
        VERIFY_ARE_EQUAL(std::wstring{ L"beta" }, after.commandText);
    }

    void CommandTimelineTests::ActionOutputRequiresLiveNativeRangeAndIsNeverCached()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 7, revision, L"ls -la", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 4);

        CommandTimelineActionModel actions;
        const auto copyOutput = actions.Prepare(CommandActionKind::CopyOutput, index.Entries(), viewState, CommandLoadPolicy{});
        VERIFY_IS_TRUE(copyOutput.Actionable());
        VERIFY_ARE_EQUAL(uint64_t{ 7 }, copyOutput.nativeMarkId);
        // The request only carries the identity needed to resolve output later.
        // No output text is ever produced or retained by the model.
        VERIFY_ARE_EQUAL(std::wstring{ L"ls -la" }, copyOutput.commandText);

        const auto jump = actions.Prepare(CommandActionKind::JumpToOutput, index.Entries(), viewState, CommandLoadPolicy{});
        VERIFY_IS_TRUE(jump.Actionable());
        VERIFY_ARE_EQUAL(uint64_t{ 7 }, jump.nativeMarkId);

        // A stale native range disables both output actions.
        auto entries = index.Entries();
        entries.front().nativeRangeValid = false;
        const auto staleCopy = actions.Prepare(CommandActionKind::CopyOutput, entries, viewState, CommandLoadPolicy{});
        const auto staleJump = actions.Prepare(CommandActionKind::JumpToOutput, entries, viewState, CommandLoadPolicy{});
        VERIFY_ARE_EQUAL(static_cast<int>(CommandActionStatus::OutputUnavailable), static_cast<int>(staleCopy.status));
        VERIFY_ARE_EQUAL(static_cast<int>(CommandActionStatus::OutputUnavailable), static_cast<int>(staleJump.status));

        // A command that never reached its output stage cannot copy output.
        // Such an entry comes from bootstrapping a mid-execution mark; a
        // 133;B-only prompt no longer produces an entry at all.
        CommandTimelineIndex pending{ PaneTwo };
        const auto pendingLoader = []() {
            return NativeBootstrapSnapshot{
                .nativeRevision = 1,
                .marks = {
                    { .nativeMarkId = 1, .commandText = L"pending", .hasCommand = true, .hasOutput = false },
                },
            };
        };
        pending.Access(1, pendingLoader);
        VERIFY_ARE_EQUAL(size_t{ 1 }, pending.Entries().size());
        CommandTimelineNavigationModel pendingNavigation;
        CommandTimelineViewState pendingView;
        pendingNavigation.Open(pending.Entries(), pendingView, pending.Capability(), 4);
        const auto pendingOutput = actions.Prepare(CommandActionKind::CopyOutput, pending.Entries(), pendingView, CommandLoadPolicy{});
        VERIFY_ARE_EQUAL(static_cast<int>(CommandActionStatus::OutputUnavailable), static_cast<int>(pendingOutput.status));
    }

    void CommandTimelineTests::SearchEmptyQueryProjectsEveryCommand()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"alpha", 0);
        Execute(index, 2, revision, L"beta", 0);
        Execute(index, 3, revision, L"gamma", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        const auto opened = navigation.Open(index.Entries(), viewState, index.Capability(), 10);
        VERIFY_ARE_EQUAL(size_t{ 3 }, opened.filteredEntryCount);
        VERIFY_IS_FALSE(opened.filtered);

        const auto cleared = navigation.SetQuery(L"", index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(size_t{ 3 }, cleared.filteredEntryCount);
        VERIFY_ARE_EQUAL(size_t{ 3 }, cleared.visibleEntries.size());
        VERIFY_IS_FALSE(cleared.filtered);
        VERIFY_ARE_EQUAL(static_cast<int>(CommandTimelineEmptyState::None), static_cast<int>(cleared.emptyState));
    }

    void CommandTimelineTests::SearchMatchesLiterallyAndCaseInsensitively()
    {
        VERIFY_IS_TRUE(CommandTimelineQueryMatches(L"Git Status", L"git"));
        VERIFY_IS_TRUE(CommandTimelineQueryMatches(L"git status", L"STATUS"));
        VERIFY_IS_TRUE(CommandTimelineQueryMatches(L"aaaa", L"aaa"));
        VERIFY_IS_TRUE(CommandTimelineQueryMatches(L"anything", L""));
        VERIFY_IS_FALSE(CommandTimelineQueryMatches(L"git", L"git status"));
        // Literal matching only: regex and glob metacharacters are just text.
        VERIFY_IS_FALSE(CommandTimelineQueryMatches(L"git status", L"g.*s"));
        VERIFY_IS_FALSE(CommandTimelineQueryMatches(L"git status", L"gs"));

        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"GIT status", 0);
        Execute(index, 2, revision, L"cargo build", 0);
        Execute(index, 3, revision, L"git log", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 10);
        const auto filtered = navigation.SetQuery(L"git", index.Entries(), viewState, index.Capability());

        VERIFY_ARE_EQUAL(size_t{ 2 }, filtered.filteredEntryCount);
        VERIFY_IS_TRUE(filtered.filtered);
        VERIFY_ARE_EQUAL(size_t{ 3 }, filtered.totalEntryCount);
        VERIFY_ARE_EQUAL(std::wstring{ L"GIT status" }, filtered.visibleEntries[0].commandText);
        VERIFY_ARE_EQUAL(std::wstring{ L"git log" }, filtered.visibleEntries[1].commandText);
    }

    void CommandTimelineTests::SearchQueryTruncationIsSurrogateSafe()
    {
        // Exactly at the boundary nothing is dropped.
        const std::wstring atLimit(MaxCommandTimelineQueryLength, L'x');
        VERIFY_ARE_EQUAL(MaxCommandTimelineQueryLength, NormalizeCommandTimelineQuery(atLimit).size());

        const std::wstring overLimit(MaxCommandTimelineQueryLength + 50, L'x');
        VERIFY_ARE_EQUAL(MaxCommandTimelineQueryLength, NormalizeCommandTimelineQuery(overLimit).size());

        // A surrogate pair straddling the boundary must not be split. U+1F600
        // is encoded as the pair D83D DE00.
        std::wstring straddling(MaxCommandTimelineQueryLength - 1, L'x');
        straddling.push_back(L'\xD83D');
        straddling.push_back(L'\xDE00');
        const auto truncated = NormalizeCommandTimelineQuery(straddling);
        VERIFY_ARE_EQUAL(MaxCommandTimelineQueryLength - 1, truncated.size());
        VERIFY_IS_FALSE(truncated.back() >= 0xD800 && truncated.back() <= 0xDBFF);

        // A pair that ends before the boundary is preserved whole.
        std::wstring safe(MaxCommandTimelineQueryLength - 2, L'y');
        safe.push_back(L'\xD83D');
        safe.push_back(L'\xDE00');
        safe.append(10, L'z');
        const auto kept = NormalizeCommandTimelineQuery(safe);
        VERIFY_ARE_EQUAL(MaxCommandTimelineQueryLength, kept.size());
        VERIFY_ARE_EQUAL(L'\xDE00', kept[MaxCommandTimelineQueryLength - 1]);

        // Non-ASCII text still matches literally.
        VERIFY_IS_TRUE(CommandTimelineQueryMatches(L"echo \x4F60\x597D", L"\x4F60\x597D"));
    }

    void CommandTimelineTests::SearchKeepsStableSelectionAcrossQueryChanges()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"git status", 0);
        Execute(index, 2, revision, L"cargo build", 0);
        Execute(index, 3, revision, L"git log", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 10);
        navigation.SelectVisibleEntry(0, index.Entries(), viewState, index.Capability());
        const auto selected = *viewState.selectedCommandId;
        VERIFY_ARE_EQUAL(uint64_t{ 1 }, selected.sequence);

        // The selected command still matches, so it stays selected.
        navigation.SetQuery(L"git", index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(selected, *viewState.selectedCommandId);

        // Clearing the query must not move the selection either.
        navigation.SetQuery(L"", index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(selected, *viewState.selectedCommandId);
    }

    void CommandTimelineTests::SearchSelectsNearestSurvivingMatch()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"git status", 0);
        Execute(index, 2, revision, L"cargo build", 0);
        Execute(index, 3, revision, L"git log", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 10);
        navigation.SelectVisibleEntry(1, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 2 }, viewState.selectedCommandId->sequence);

        // "cargo build" no longer matches, so the nearest surviving match takes
        // the selection rather than dropping to the bottom.
        const auto filtered = navigation.SetQuery(L"git", index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(size_t{ 2 }, filtered.filteredEntryCount);
        VERIFY_ARE_EQUAL(uint64_t{ 1 }, viewState.selectedCommandId->sequence);

        // A query with no results leaves the selected command untouched.
        const auto none = navigation.SetQuery(L"zzz", index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(size_t{ 0 }, none.filteredEntryCount);
        VERIFY_IS_TRUE(none.visibleEntries.empty());
        VERIFY_ARE_EQUAL(static_cast<int>(CommandTimelineEmptyState::NoMatchingCommands),
                         static_cast<int>(none.emptyState));
        VERIFY_ARE_EQUAL(uint64_t{ 1 }, viewState.selectedCommandId->sequence);
    }

    void CommandTimelineTests::SearchNavigationAndWheelWalkFilteredProjection()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        for (uint64_t mark = 1; mark <= 10; ++mark)
        {
            // Odd marks match the query, even marks do not.
            Execute(index, mark, revision, mark % 2 ? L"git command" : L"other command", 0);
        }

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 3);
        const auto filtered = navigation.SetQuery(L"git", index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(size_t{ 5 }, filtered.filteredEntryCount);
        VERIFY_ARE_EQUAL(uint64_t{ 9 }, viewState.selectedCommandId->sequence);

        // Up moves to the previous *match*, skipping the non-matching command.
        navigation.Navigate(NavigationAction::Previous, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 7 }, viewState.selectedCommandId->sequence);
        navigation.Navigate(NavigationAction::Previous, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 5 }, viewState.selectedCommandId->sequence);
        navigation.Navigate(NavigationAction::Next, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 7 }, viewState.selectedCommandId->sequence);

        // Page edges stay inside the filtered viewport.
        const auto pageFirst = navigation.Navigate(NavigationAction::PageFirst, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(size_t{ 0 }, pageFirst.selectedVisualSlot);
        const auto pageLast = navigation.Navigate(NavigationAction::PageLast, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(size_t{ 2 }, pageLast.selectedVisualSlot);

        // Wheel accumulation also walks matches only. A partial delta moves
        // nothing; a full notch moves exactly one match.
        const auto beforeWheel = viewState.selectedCommandId->sequence;
        navigation.ApplyWheelDelta(40, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(beforeWheel, viewState.selectedCommandId->sequence);
        navigation.ApplyWheelDelta(80, index.Entries(), viewState, index.Capability());
        VERIFY_ARE_NOT_EQUAL(beforeWheel, viewState.selectedCommandId->sequence);
        VERIFY_ARE_EQUAL(uint64_t{ 1 }, viewState.selectedCommandId->sequence % 2);
    }

    void CommandTimelineTests::SearchNewCommandFollowsLatestOnlyWhenMatching()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"git one", 0);
        Execute(index, 2, revision, L"git two", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 10);
        navigation.SetQuery(L"git", index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(uint64_t{ 2 }, viewState.selectedCommandId->sequence);

        // Following latest: a new matching command becomes the selection.
        Execute(index, 3, revision, L"git three", 0);
        navigation.Reconcile(index.Entries(), viewState, index.Capability(), 10);
        VERIFY_ARE_EQUAL(uint64_t{ 3 }, viewState.selectedCommandId->sequence);

        // A new command that does not match must not change the selection.
        Execute(index, 4, revision, L"unrelated", 0);
        const auto afterNonMatching = navigation.Reconcile(index.Entries(), viewState, index.Capability(), 10);
        VERIFY_ARE_EQUAL(uint64_t{ 3 }, viewState.selectedCommandId->sequence);
        VERIFY_ARE_EQUAL(size_t{ 3 }, afterNonMatching.filteredEntryCount);

        // Browsing older history: a new matching command must not yank the
        // selection back to the bottom.
        navigation.Navigate(NavigationAction::Previous, index.Entries(), viewState, index.Capability());
        const auto browsing = viewState.selectedCommandId->sequence;
        VERIFY_ARE_EQUAL(uint64_t{ 2 }, browsing);
        Execute(index, 5, revision, L"git five", 0);
        navigation.Reconcile(index.Entries(), viewState, index.Capability(), 10);
        VERIFY_ARE_EQUAL(browsing, viewState.selectedCommandId->sequence);
    }

    void CommandTimelineTests::ShellDegradationStatesAreDistinct()
    {
        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;

        // Capability not yet determined and nothing recorded.
        const auto waiting = navigation.Open({}, viewState, ShellIntegrationCapability::Unknown, 5);
        VERIFY_ARE_EQUAL(static_cast<int>(CommandTimelineEmptyState::WaitingForShell),
                         static_cast<int>(waiting.emptyState));

        // Shell cannot report complete boundaries.
        CommandTimelineNavigationModel limitedNavigation;
        CommandTimelineViewState limitedView;
        const auto unsupported = limitedNavigation.Open({}, limitedView, ShellIntegrationCapability::Limited, 5);
        VERIFY_ARE_EQUAL(static_cast<int>(CommandTimelineEmptyState::ShellUnsupported),
                         static_cast<int>(unsupported.emptyState));

        // Shell integration works but no command has run yet.
        CommandTimelineNavigationModel fullNavigation;
        CommandTimelineViewState fullView;
        const auto noCommands = fullNavigation.Open({}, fullView, ShellIntegrationCapability::Full, 5);
        VERIFY_ARE_EQUAL(static_cast<int>(CommandTimelineEmptyState::NoCommands),
                         static_cast<int>(noCommands.emptyState));

        // Commands exist but the filter excludes all of them.
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"present", 0);
        CommandTimelineNavigationModel filteredNavigation;
        CommandTimelineViewState filteredView;
        filteredNavigation.Open(index.Entries(), filteredView, index.Capability(), 5);
        const auto noMatches = filteredNavigation.SetQuery(L"absent", index.Entries(), filteredView, index.Capability());
        VERIFY_ARE_EQUAL(static_cast<int>(CommandTimelineEmptyState::NoMatchingCommands),
                         static_cast<int>(noMatches.emptyState));
    }

    void CommandTimelineTests::HistoryLimitEvictsOldestFirstAndNeverResurrects()
    {
        CommandTimelineIndex index{ PaneOne };
        index.SetHistoryLimit(MinCommandTimelineHistoryLimit);
        VERIFY_ARE_EQUAL(MinCommandTimelineHistoryLimit, index.HistoryLimit());

        uint64_t revision = 0;
        for (uint64_t mark = 1; mark <= 60; ++mark)
        {
            Execute(index, mark, revision, L"command " + std::to_wstring(mark), 0);
        }

        // Bounded at the limit, oldest dropped first, newest retained.
        VERIFY_ARE_EQUAL(MinCommandTimelineHistoryLimit, index.Entries().size());
        VERIFY_ARE_EQUAL(uint64_t{ 11 }, index.Entries().front().id.sequence);
        VERIFY_ARE_EQUAL(uint64_t{ 60 }, index.Entries().back().id.sequence);

        // Lowering the limit evicts immediately.
        CommandTimelineIndex lowered{ PaneTwo };
        uint64_t loweredRevision = 0;
        for (uint64_t mark = 1; mark <= 120; ++mark)
        {
            Execute(lowered, mark, loweredRevision, L"cmd", 0);
        }
        VERIFY_ARE_EQUAL(size_t{ 120 }, lowered.Entries().size());
        lowered.SetHistoryLimit(MinCommandTimelineHistoryLimit);
        VERIFY_ARE_EQUAL(MinCommandTimelineHistoryLimit, lowered.Entries().size());
        VERIFY_ARE_EQUAL(uint64_t{ 71 }, lowered.Entries().front().id.sequence);

        // Raising the limit must not resurrect anything, and sequence numbers
        // are never reissued.
        lowered.SetHistoryLimit(1000);
        VERIFY_ARE_EQUAL(MinCommandTimelineHistoryLimit, lowered.Entries().size());
        VERIFY_ARE_EQUAL(uint64_t{ 71 }, lowered.Entries().front().id.sequence);
        VERIFY_ARE_EQUAL(uint64_t{ 121 }, lowered.NextSequence());
        Execute(lowered, 500, loweredRevision, L"after raise", 0);
        VERIFY_ARE_EQUAL(uint64_t{ 121 }, lowered.Entries().back().id.sequence);
    }

    void CommandTimelineTests::HistoryLimitClampsToSupportedRange()
    {
        VERIFY_ARE_EQUAL(DefaultCommandTimelineHistoryLimit, size_t{ 500 });
        VERIFY_ARE_EQUAL(MinCommandTimelineHistoryLimit, ClampCommandTimelineHistoryLimit(0));
        VERIFY_ARE_EQUAL(MinCommandTimelineHistoryLimit, ClampCommandTimelineHistoryLimit(-1));
        VERIFY_ARE_EQUAL(MinCommandTimelineHistoryLimit, ClampCommandTimelineHistoryLimit(49));
        VERIFY_ARE_EQUAL(MinCommandTimelineHistoryLimit, ClampCommandTimelineHistoryLimit(50));
        VERIFY_ARE_EQUAL(size_t{ 500 }, ClampCommandTimelineHistoryLimit(500));
        VERIFY_ARE_EQUAL(MaxCommandTimelineHistoryLimit, ClampCommandTimelineHistoryLimit(5000));
        VERIFY_ARE_EQUAL(MaxCommandTimelineHistoryLimit, ClampCommandTimelineHistoryLimit(999999));

        // An out-of-range configured value is clamped rather than rejected.
        CommandTimelineIndex index{ PaneOne };
        index.SetHistoryLimit(ClampCommandTimelineHistoryLimit(1));
        VERIFY_ARE_EQUAL(MinCommandTimelineHistoryLimit, index.HistoryLimit());
        index.SetHistoryLimit(ClampCommandTimelineHistoryLimit(100000));
        VERIFY_ARE_EQUAL(MaxCommandTimelineHistoryLimit, index.HistoryLimit());
    }

    void CommandTimelineTests::SearchCloseReleasesQueryAndProjection()
    {
        CommandTimelineIndex index{ PaneOne };
        uint64_t revision = 0;
        Execute(index, 1, revision, L"git status", 0);
        Execute(index, 2, revision, L"cargo build", 0);

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        navigation.Open(index.Entries(), viewState, index.Capability(), 5);
        navigation.SetQuery(L"git", index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(std::wstring{ L"git" }, navigation.Query());
        VERIFY_ARE_EQUAL(size_t{ 1 }, navigation.FilteredCount());

        navigation.Close();
        VERIFY_IS_TRUE(navigation.Query().empty());
        VERIFY_ARE_EQUAL(size_t{ 0 }, navigation.FilteredCount());
        VERIFY_IS_FALSE(navigation.IsOpen());
        VERIFY_IS_FALSE(navigation.WheelSettlePending());
        VERIFY_ARE_EQUAL(0, navigation.WheelDeltaRemainder());

        // Repeated close stays safe.
        navigation.Close();
        VERIFY_IS_FALSE(navigation.IsOpen());

        // Reopening starts unfiltered: a query is never persisted.
        const auto reopened = navigation.Open(index.Entries(), viewState, index.Capability(), 5);
        VERIFY_IS_TRUE(navigation.Query().empty());
        VERIFY_IS_FALSE(reopened.filtered);
        VERIFY_ARE_EQUAL(size_t{ 2 }, reopened.filteredEntryCount);
    }

    void CommandTimelineTests::SearchStressAtMaximumHistoryLimit()
    {
        CommandTimelineIndex index{ PaneOne };
        index.SetHistoryLimit(MaxCommandTimelineHistoryLimit);

        uint64_t revision = 0;
        for (uint64_t mark = 1; mark <= MaxCommandTimelineHistoryLimit; ++mark)
        {
            Execute(index, mark, revision, L"command " + std::to_wstring(mark), 0);
        }
        VERIFY_ARE_EQUAL(MaxCommandTimelineHistoryLimit, index.Entries().size());

        CommandTimelineNavigationModel navigation;
        CommandTimelineViewState viewState;
        const auto capacity = size_t{ 20 };
        navigation.Open(index.Entries(), viewState, index.Capability(), capacity);

        // Worst case: a query that matches every entry. Only the visible rows
        // are ever materialized.
        const auto broad = navigation.SetQuery(L"command", index.Entries(), viewState, index.Capability());
        VERIFY_ARE_EQUAL(MaxCommandTimelineHistoryLimit, broad.filteredEntryCount);
        VERIFY_ARE_EQUAL(capacity, broad.visibleEntries.size());

        // Repeated filtering must stay bounded and must not accumulate.
        for (int pass = 0; pass < 25; ++pass)
        {
            const auto narrow = navigation.SetQuery(L"command 4242", index.Entries(), viewState, index.Capability());
            VERIFY_ARE_EQUAL(size_t{ 1 }, narrow.filteredEntryCount);
            VERIFY_ARE_EQUAL(size_t{ 1 }, narrow.visibleEntries.size());

            const auto missing = navigation.SetQuery(L"no such command", index.Entries(), viewState, index.Capability());
            VERIFY_ARE_EQUAL(size_t{ 0 }, missing.filteredEntryCount);
            VERIFY_IS_TRUE(missing.visibleEntries.empty());

            const auto all = navigation.SetQuery(L"", index.Entries(), viewState, index.Capability());
            VERIFY_ARE_EQUAL(MaxCommandTimelineHistoryLimit, all.filteredEntryCount);
            VERIFY_ARE_EQUAL(capacity, all.visibleEntries.size());
        }

        // Command text stays bounded and no output is ever cached.
        VERIFY_IS_TRUE(index.CachedCommandTextCharacters() <=
                       MaxCommandTimelineHistoryLimit * DefaultMaxCachedCommandText);

        navigation.Close();
        VERIFY_ARE_EQUAL(size_t{ 0 }, navigation.FilteredCount());
    }
}
