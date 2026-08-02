// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <winrt/base.h>

namespace winTerm::CommandTimeline
{
    inline constexpr size_t DefaultMaxCachedCommandText = 4096;

    enum class CommandLifecycleState : uint8_t
    {
        Command,
        Output,
        Completed,
        Incomplete,
    };

    enum class ExecutionResult : uint8_t
    {
        Running,
        Succeeded,
        Failed,
        Cancelled,
        Unknown,
    };

    enum class ShellIntegrationCapability : uint8_t
    {
        Full,
        Limited,
        Unknown,
    };

    enum class LifecycleEventKind : uint8_t
    {
        Prompt,
        CommandStart,
        OutputStart,
        CommandFinished,
        CommandCancelled,
    };

    struct CommandId
    {
        winrt::guid paneSessionId{};
        uint64_t sequence{};

        bool operator==(const CommandId&) const noexcept = default;
    };

    struct CommandTimelineEntry
    {
        CommandId id{};
        std::wstring cachedCommandText;
        uint64_t nativeMarkId{};
        CommandLifecycleState lifecycleState{ CommandLifecycleState::Command };
        ExecutionResult executionResult{ ExecutionResult::Unknown };
        std::optional<uint32_t> trustedExitCode;
        std::optional<std::chrono::system_clock::time_point> startTimestamp;
        std::optional<std::chrono::system_clock::time_point> endTimestamp;
        bool nativeRangeValid{ true };
        ShellIntegrationCapability shellIntegrationCapability{ ShellIntegrationCapability::Unknown };
    };

    struct CommandTimelineViewState
    {
        std::optional<CommandId> selectedCommandId;
        std::optional<uint64_t> visibleNativeAnchor;
        std::optional<size_t> selectedVisualSlot;
        bool loadedIntoInput{ false };
        uint64_t executionGeneration{};

        void Reset() noexcept;
    };

    struct CommandTimelineSnapshot
    {
        winrt::guid paneSessionId{};
        std::vector<CommandTimelineEntry> entries;
        CommandTimelineViewState viewState;
        ShellIntegrationCapability capability{ ShellIntegrationCapability::Unknown };
        uint64_t revision{};
        uint64_t nativeRevision{};
        size_t bootstrapScanCount{};
        bool closed{ false };
    };

    enum class NavigationAction : uint8_t
    {
        Previous,
        Next,
        PageFirst,
        PageLast,
    };

    struct CommandTimelineVisibleEntry
    {
        CommandId id{};
        std::wstring commandText;
        ExecutionResult executionResult{ ExecutionResult::Unknown };
        bool selected{ false };
    };

    struct CommandTimelinePresentationSnapshot
    {
        std::vector<CommandTimelineVisibleEntry> visibleEntries;
        ShellIntegrationCapability capability{ ShellIntegrationCapability::Unknown };
        size_t totalEntryCount{};
        size_t firstVisibleIndex{};
        size_t selectedVisualSlot{};
        int wheelDeltaRemainder{};
        bool open{ false };
        bool wheelSettlePending{ false };
    };

    class CommandTimelineNavigationModel final
    {
    public:
        static constexpr int DefaultWheelDeltaPerEntry = 120;

        CommandTimelinePresentationSnapshot Open(std::span<const CommandTimelineEntry> entries,
                                                 CommandTimelineViewState& viewState,
                                                 ShellIntegrationCapability capability,
                                                 size_t visibleCapacity);
        CommandTimelinePresentationSnapshot Reconcile(std::span<const CommandTimelineEntry> entries,
                                                      CommandTimelineViewState& viewState,
                                                      ShellIntegrationCapability capability,
                                                      size_t visibleCapacity);
        CommandTimelinePresentationSnapshot Navigate(NavigationAction action,
                                                     std::span<const CommandTimelineEntry> entries,
                                                     CommandTimelineViewState& viewState,
                                                     ShellIntegrationCapability capability);
        CommandTimelinePresentationSnapshot SelectVisibleEntry(size_t visualSlot,
                                                               std::span<const CommandTimelineEntry> entries,
                                                               CommandTimelineViewState& viewState,
                                                               ShellIntegrationCapability capability);
        CommandTimelinePresentationSnapshot ApplyWheelDelta(int delta,
                                                            std::span<const CommandTimelineEntry> entries,
                                                            CommandTimelineViewState& viewState,
                                                            ShellIntegrationCapability capability,
                                                            int deltaPerEntry = DefaultWheelDeltaPerEntry);
        void SettleWheel() noexcept;
        void Close() noexcept;

        bool IsOpen() const noexcept;
        bool WheelSettlePending() const noexcept;
        int WheelDeltaRemainder() const noexcept;
        size_t VisibleCapacity() const noexcept;

    private:
        void _reconcile(std::span<const CommandTimelineEntry> entries,
                        CommandTimelineViewState& viewState,
                        bool allowFollowLatest);
        bool _moveSelection(NavigationAction action,
                            std::span<const CommandTimelineEntry> entries);
        void _syncViewState(std::span<const CommandTimelineEntry> entries,
                            CommandTimelineViewState& viewState) const;
        CommandTimelinePresentationSnapshot _snapshot(std::span<const CommandTimelineEntry> entries,
                                                      ShellIntegrationCapability capability) const;
        static std::optional<size_t> _findCommand(std::span<const CommandTimelineEntry> entries,
                                                  const CommandId& id) noexcept;
        static size_t _findNearestCommand(std::span<const CommandTimelineEntry> entries,
                                          const CommandId& id) noexcept;
        static ExecutionResult _effectiveResult(const CommandTimelineEntry& entry) noexcept;

        std::optional<size_t> _selectedIndex;
        std::optional<CommandId> _lastLatestCommandId;
        size_t _firstVisibleIndex{};
        size_t _visibleCapacity{ 1 };
        int _wheelDeltaRemainder{};
        bool _open{ false };
        bool _wheelSettlePending{ false };
    };

    struct NativeMarkSnapshot
    {
        uint64_t nativeMarkId{};
        std::wstring commandText;
        bool hasCommand{ false };
        bool hasOutput{ false };
        bool nativeRangeValid{ true };
        std::optional<uint32_t> trustedExitCode;
    };

    struct NativeBootstrapSnapshot
    {
        uint64_t nativeRevision{};
        std::vector<NativeMarkSnapshot> marks;
    };

    struct LifecycleUpdate
    {
        LifecycleEventKind kind{};
        uint64_t nativeMarkId{};
        uint64_t nativeRevision{};
        std::wstring_view commandText;
        std::optional<uint32_t> trustedExitCode;
        std::optional<std::chrono::system_clock::time_point> timestamp;
    };

    struct CommandTextCachePolicy
    {
        size_t maxCommandTextLength{ DefaultMaxCachedCommandText };

        std::wstring Apply(std::wstring_view commandText) const;
    };

    class CommandTimelineIndex final
    {
    public:
        using BootstrapLoader = std::function<NativeBootstrapSnapshot()>;

        explicit CommandTimelineIndex(winrt::guid paneSessionId,
                                      CommandTextCachePolicy cachePolicy = {});

        const std::vector<CommandTimelineEntry>& Access(uint64_t nativeRevision,
                                                        const BootstrapLoader& loader);
        void ProcessLifecycle(const LifecycleUpdate& update);
        void InvalidateNativeMarks(std::span<const uint64_t> nativeMarkIds,
                                   uint64_t nativeRevision);
        void ReconcileReflow(std::span<const uint64_t> survivingNativeMarkIds,
                             uint64_t nativeRevision);
        void SetCapabilityFallback(ShellIntegrationCapability capability);
        void Close() noexcept;

        const CommandTimelineEntry* Find(const CommandId& id) const noexcept;
        const CommandTimelineEntry* Current() const noexcept;
        const std::vector<CommandTimelineEntry>& Entries() const noexcept;
        const winrt::guid& PaneSessionId() const noexcept;
        ShellIntegrationCapability Capability() const noexcept;
        uint64_t Revision() const noexcept;
        uint64_t NativeRevision() const noexcept;
        uint64_t NextSequence() const noexcept;
        size_t BootstrapScanCount() const noexcept;
        size_t CachedCommandTextCharacters() const noexcept;
        bool IsBootstrapped() const noexcept;
        bool IsClosed() const noexcept;

    private:
        CommandTimelineEntry& _createEntry(uint64_t nativeMarkId,
                                           std::optional<std::chrono::system_clock::time_point> timestamp);
        void _applyBootstrap(NativeBootstrapSnapshot snapshot);
        void _finishIncompleteCurrent(std::optional<std::chrono::system_clock::time_point> timestamp);
        void _observeLifecycleCapability(uint64_t nativeMarkId, LifecycleEventKind kind);
        void _refreshEntryCapabilities();
        void _rebuildLookups();
        void _incrementRevision() noexcept;

        winrt::guid _paneSessionId{};
        CommandTextCachePolicy _cachePolicy{};
        std::vector<CommandTimelineEntry> _entries;
        std::unordered_map<uint64_t, size_t> _byNativeMark;
        std::unordered_map<uint64_t, size_t> _bySequence;
        std::unordered_map<uint64_t, uint8_t> _lifecycleByNativeMark;
        std::optional<uint64_t> _currentSequence;
        uint64_t _nextSequence{ 1 };
        uint64_t _revision{};
        uint64_t _nativeRevision{};
        size_t _bootstrapScanCount{};
        ShellIntegrationCapability _capability{ ShellIntegrationCapability::Unknown };
        bool _bootstrapped{ false };
        bool _closed{ false };
    };

    ShellIntegrationCapability InferCapabilityFallbackFromCommandline(std::wstring_view commandline) noexcept;
}
