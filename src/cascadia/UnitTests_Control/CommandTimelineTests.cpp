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
        index.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, 11, 1));
        const auto revisionAfterStart = index.Revision();
        index.ProcessLifecycle(Update(LifecycleEventKind::CommandStart, 11, 1));
        VERIFY_ARE_EQUAL(size_t{ 1 }, index.Entries().size());
        VERIFY_ARE_EQUAL(revisionAfterStart, index.Revision());

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
}
