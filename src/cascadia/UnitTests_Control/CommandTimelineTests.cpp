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
    }
}
