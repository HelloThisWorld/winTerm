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

    // Phase 4 search and history bounds. The query cap is expressed in UTF-16
    // code units because that is what the input box and the cached command text
    // are measured in.
    inline constexpr size_t MaxCommandTimelineQueryLength = 256;
    inline constexpr size_t DefaultCommandTimelineHistoryLimit = 500;
    inline constexpr size_t MinCommandTimelineHistoryLimit = 50;
    inline constexpr size_t MaxCommandTimelineHistoryLimit = 5000;

    // Truncates a query to MaxCommandTimelineQueryLength without ever leaving a
    // lone surrogate behind.
    std::wstring NormalizeCommandTimelineQuery(std::wstring_view query);

    // Literal, case-insensitive substring match. No regex, no fuzzy matching,
    // and only ever applied to cached command text.
    bool CommandTimelineQueryMatches(std::wstring_view commandText, std::wstring_view query) noexcept;

    // Clamps a configured history limit into the supported range.
    size_t ClampCommandTimelineHistoryLimit(int64_t historyLimit) noexcept;

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
        std::optional<CommandId> loadedCommandId;
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

    // Which "nothing to show" message the overlay should present. These are
    // distinct states on purpose: an unsupported shell must never be reported
    // as simply having run no commands.
    enum class CommandTimelineEmptyState : uint8_t
    {
        None,
        WaitingForShell,
        ShellUnsupported,
        NoCommands,
        NoMatchingCommands,
    };

    struct CommandTimelinePresentationSnapshot
    {
        std::vector<CommandTimelineVisibleEntry> visibleEntries;
        ShellIntegrationCapability capability{ ShellIntegrationCapability::Unknown };
        CommandTimelineEmptyState emptyState{ CommandTimelineEmptyState::None };
        size_t totalEntryCount{};
        size_t filteredEntryCount{};
        size_t firstVisibleIndex{};
        size_t selectedVisualSlot{};
        int wheelDeltaRemainder{};
        bool open{ false };
        bool filtered{ false };
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
        CommandTimelinePresentationSnapshot SetQuery(std::wstring_view query,
                                                     std::span<const CommandTimelineEntry> entries,
                                                     CommandTimelineViewState& viewState,
                                                     ShellIntegrationCapability capability);
        void SettleWheel() noexcept;
        void Close() noexcept;

        bool IsOpen() const noexcept;
        bool WheelSettlePending() const noexcept;
        int WheelDeltaRemainder() const noexcept;
        size_t VisibleCapacity() const noexcept;
        const std::wstring& Query() const noexcept;
        size_t FilteredCount() const noexcept;

    private:
        void _reconcile(std::span<const CommandTimelineEntry> entries,
                        CommandTimelineViewState& viewState,
                        bool allowFollowLatest);
        void _rebuildFilter(std::span<const CommandTimelineEntry> entries);
        bool _moveSelection(NavigationAction action);
        void _syncViewState(std::span<const CommandTimelineEntry> entries,
                            CommandTimelineViewState& viewState) const;
        CommandTimelinePresentationSnapshot _snapshot(std::span<const CommandTimelineEntry> entries,
                                                      ShellIntegrationCapability capability) const;
        std::optional<size_t> _positionOf(size_t entryIndex) const noexcept;
        size_t _nearestPosition(std::span<const CommandTimelineEntry> entries,
                                const CommandId& id) const noexcept;
        static std::optional<size_t> _findCommand(std::span<const CommandTimelineEntry> entries,
                                                  const CommandId& id) noexcept;
        static size_t _findNearestCommand(std::span<const CommandTimelineEntry> entries,
                                          const CommandId& id) noexcept;
        static ExecutionResult _effectiveResult(const CommandTimelineEntry& entry) noexcept;

        // Positions into _filtered, which holds indices into the caller's
        // entries span. An empty query keeps every entry, so the unfiltered
        // case walks the same code path.
        std::vector<size_t> _filtered;
        std::wstring _query;
        std::optional<size_t> _selectedPosition;
        std::optional<CommandId> _lastLatestCommandId;
        size_t _firstVisiblePosition{};
        size_t _visibleCapacity{ 1 };
        int _wheelDeltaRemainder{};
        bool _open{ false };
        bool _wheelSettlePending{ false };
    };

    enum class CommandActionKind : uint8_t
    {
        LoadIntoInput,
        CopyCommand,
        CopyOutput,
        JumpToOutput,
    };

    enum class CommandActionStatus : uint8_t
    {
        Ready,
        NoSelection,
        CommandTextUnavailable,
        OutputUnavailable,
        ConfirmationRequired,
        MultilineUnsafe,
    };

    // Describes how a Command Timeline entry may be loaded into the owning
    // pane's input. The Timeline never reads the Windows clipboard, so the
    // multiline and large-input protections that guard clipboard paste are
    // reproduced here against the bounded command-text cache instead.
    struct CommandLoadPolicy
    {
        size_t largeLoadCharacterThreshold{ 1024 };
        bool bracketedPasteEnabled{ false };
        bool warnOnMultilineLoad{ true };
        bool warnOnLargeLoad{ true };
    };

    struct CommandActionRequest
    {
        CommandActionKind kind{ CommandActionKind::LoadIntoInput };
        CommandActionStatus status{ CommandActionStatus::NoSelection };
        CommandId id{};
        uint64_t nativeMarkId{};
        std::wstring commandText;
        uint64_t executionGeneration{};
        size_t characterCount{};
        bool multiline{ false };

        bool Actionable() const noexcept;
    };

    // Pure decision layer for the Phase 3 entry actions. It never resolves
    // command output, never touches the clipboard, and never appends a
    // carriage return, so a prepared load can only ever populate the shell's
    // input line.
    class CommandTimelineActionModel final
    {
    public:
        CommandActionRequest Prepare(CommandActionKind kind,
                                     std::span<const CommandTimelineEntry> entries,
                                     const CommandTimelineViewState& viewState,
                                     const CommandLoadPolicy& policy) const;
        static CommandActionRequest Confirm(CommandActionRequest request) noexcept;

        void NotifyLoaded(CommandTimelineViewState& viewState, const CommandActionRequest& request) noexcept;
        void NotifyExecutionStarted(CommandTimelineViewState& viewState) noexcept;
        void ReconcileLoadedInput(std::span<const CommandTimelineEntry> entries,
                                  CommandTimelineViewState& viewState) noexcept;
        static bool IsCurrentGeneration(const CommandTimelineViewState& viewState,
                                        uint64_t executionGeneration) noexcept;
        void Reset(CommandTimelineViewState& viewState) noexcept;

        static bool IsMultiline(std::wstring_view commandText) noexcept;

    private:
        static const CommandTimelineEntry* _selected(std::span<const CommandTimelineEntry> entries,
                                                     const CommandTimelineViewState& viewState) noexcept;
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
        void SetHistoryLimit(size_t historyLimit);
        void Close() noexcept;

        const CommandTimelineEntry* Find(const CommandId& id) const noexcept;
        const CommandTimelineEntry* Current() const noexcept;
        const std::vector<CommandTimelineEntry>& Entries() const noexcept;
        const winrt::guid& PaneSessionId() const noexcept;
        ShellIntegrationCapability Capability() const noexcept;
        uint64_t Revision() const noexcept;
        uint64_t NativeRevision() const noexcept;
        uint64_t NextSequence() const noexcept;
        size_t HistoryLimit() const noexcept;
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
        bool _applyHistoryLimit();
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
        size_t _historyLimit{ DefaultCommandTimelineHistoryLimit };
        uint64_t _revision{};
        uint64_t _nativeRevision{};
        size_t _bootstrapScanCount{};
        ShellIntegrationCapability _capability{ ShellIntegrationCapability::Unknown };
        bool _bootstrapped{ false };
        bool _closed{ false };
    };

    ShellIntegrationCapability InferCapabilityFallbackFromCommandline(std::wstring_view commandline) noexcept;
}
