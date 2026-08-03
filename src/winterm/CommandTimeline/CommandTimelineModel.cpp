// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "CommandTimelineModel.h"

#include <algorithm>
#include <cwctype>
#include <limits>
#include <unordered_set>

namespace winTerm::CommandTimeline
{
    namespace
    {
        constexpr uint8_t PromptEvent = 0x01;
        constexpr uint8_t CommandStartEvent = 0x02;
        constexpr uint8_t OutputStartEvent = 0x04;
        constexpr uint8_t CommandFinishedEvent = 0x08;
        constexpr uint8_t FullLifecycle = PromptEvent | CommandStartEvent | OutputStartEvent | CommandFinishedEvent;

        bool IsHighSurrogate(const wchar_t value) noexcept
        {
            return value >= 0xD800 && value <= 0xDBFF;
        }

        bool IsLowSurrogate(const wchar_t value) noexcept
        {
            return value >= 0xDC00 && value <= 0xDFFF;
        }
    }

    std::wstring NormalizeCommandTimelineQuery(const std::wstring_view query)
    {
        if (query.size() <= MaxCommandTimelineQueryLength)
        {
            return std::wstring{ query };
        }

        std::wstring result{ query.substr(0, MaxCommandTimelineQueryLength) };
        // Never cut a surrogate pair in half; drop the orphaned lead unit.
        if (!result.empty() && IsHighSurrogate(result.back()))
        {
            result.pop_back();
        }
        return result;
    }

    bool CommandTimelineQueryMatches(const std::wstring_view commandText,
                                     const std::wstring_view query) noexcept
    {
        if (query.empty())
        {
            return true;
        }
        if (commandText.size() < query.size())
        {
            return false;
        }

        // Literal case-insensitive substring search. Deliberately not a regex
        // and deliberately not fuzzy.
        const auto found = std::search(
            commandText.begin(), commandText.end(), query.begin(), query.end(), [](const wchar_t left, const wchar_t right) noexcept {
                return std::towlower(left) == std::towlower(right);
            });
        return found != commandText.end();
    }

    size_t ClampCommandTimelineHistoryLimit(const int64_t historyLimit) noexcept
    {
        if (historyLimit <= static_cast<int64_t>(MinCommandTimelineHistoryLimit))
        {
            return MinCommandTimelineHistoryLimit;
        }
        if (historyLimit >= static_cast<int64_t>(MaxCommandTimelineHistoryLimit))
        {
            return MaxCommandTimelineHistoryLimit;
        }
        return static_cast<size_t>(historyLimit);
    }

    void CommandTimelineViewState::Reset() noexcept
    {
        selectedCommandId.reset();
        visibleNativeAnchor.reset();
        selectedVisualSlot.reset();
        loadedCommandId.reset();
        loadedIntoInput = false;
        executionGeneration = 0;
    }

    CommandTimelinePresentationSnapshot CommandTimelineNavigationModel::Open(
        const std::span<const CommandTimelineEntry> entries,
        CommandTimelineViewState& viewState,
        const ShellIntegrationCapability capability,
        const size_t visibleCapacity)
    {
        _open = true;
        _visibleCapacity = std::max<size_t>(1, visibleCapacity);
        _wheelDeltaRemainder = 0;
        _wheelSettlePending = false;
        _reconcile(entries, viewState, false);
        return _snapshot(entries, capability);
    }

    CommandTimelinePresentationSnapshot CommandTimelineNavigationModel::Reconcile(
        const std::span<const CommandTimelineEntry> entries,
        CommandTimelineViewState& viewState,
        const ShellIntegrationCapability capability,
        const size_t visibleCapacity)
    {
        _visibleCapacity = std::max<size_t>(1, visibleCapacity);
        if (_open)
        {
            _reconcile(entries, viewState, true);
        }
        return _snapshot(entries, capability);
    }

    CommandTimelinePresentationSnapshot CommandTimelineNavigationModel::Navigate(
        const NavigationAction action,
        const std::span<const CommandTimelineEntry> entries,
        CommandTimelineViewState& viewState,
        const ShellIntegrationCapability capability)
    {
        if (_open)
        {
            _reconcile(entries, viewState, false);
            _moveSelection(action);
            _syncViewState(entries, viewState);
        }
        return _snapshot(entries, capability);
    }

    CommandTimelinePresentationSnapshot CommandTimelineNavigationModel::SelectVisibleEntry(
        const size_t visualSlot,
        const std::span<const CommandTimelineEntry> entries,
        CommandTimelineViewState& viewState,
        const ShellIntegrationCapability capability)
    {
        if (_open)
        {
            _reconcile(entries, viewState, false);
            const auto position = _firstVisiblePosition + visualSlot;
            if (visualSlot < _visibleCapacity && position < _filtered.size())
            {
                _selectedPosition = position;
                _syncViewState(entries, viewState);
            }
        }
        return _snapshot(entries, capability);
    }

    // Applying a query only changes which commands are projected. The selected
    // command is preserved whenever it still matches, so typing never silently
    // retargets an action.
    CommandTimelinePresentationSnapshot CommandTimelineNavigationModel::SetQuery(
        const std::wstring_view query,
        const std::span<const CommandTimelineEntry> entries,
        CommandTimelineViewState& viewState,
        const ShellIntegrationCapability capability)
    {
        auto normalized = NormalizeCommandTimelineQuery(query);
        if (normalized != _query)
        {
            _query = std::move(normalized);
            _wheelDeltaRemainder = 0;
        }

        if (_open)
        {
            _reconcile(entries, viewState, false);
        }
        return _snapshot(entries, capability);
    }

    CommandTimelinePresentationSnapshot CommandTimelineNavigationModel::ApplyWheelDelta(
        const int delta,
        const std::span<const CommandTimelineEntry> entries,
        CommandTimelineViewState& viewState,
        const ShellIntegrationCapability capability,
        const int deltaPerEntry)
    {
        if (!_open || entries.empty() || delta == 0 || deltaPerEntry <= 0)
        {
            return _snapshot(entries, capability);
        }

        _reconcile(entries, viewState, false);
        if (_filtered.empty())
        {
            return _snapshot(entries, capability);
        }
        _wheelSettlePending = true;
        const auto accumulated = static_cast<int64_t>(_wheelDeltaRemainder) + delta;
        _wheelDeltaRemainder = gsl::narrow_cast<int>(std::clamp<int64_t>(
            accumulated,
            std::numeric_limits<int>::min(),
            std::numeric_limits<int>::max()));

        while (_wheelDeltaRemainder >= deltaPerEntry)
        {
            _wheelDeltaRemainder -= deltaPerEntry;
            if (!_moveSelection(NavigationAction::Previous))
            {
                _wheelDeltaRemainder = 0;
                break;
            }
        }
        while (_wheelDeltaRemainder <= -deltaPerEntry)
        {
            _wheelDeltaRemainder += deltaPerEntry;
            if (!_moveSelection(NavigationAction::Next))
            {
                _wheelDeltaRemainder = 0;
                break;
            }
        }

        _syncViewState(entries, viewState);
        return _snapshot(entries, capability);
    }

    void CommandTimelineNavigationModel::SettleWheel() noexcept
    {
        _wheelDeltaRemainder = 0;
        _wheelSettlePending = false;
    }

    void CommandTimelineNavigationModel::Close() noexcept
    {
        // Closing drops the query and the filtered projection along with the
        // rest of the UI-only state; a query is never persisted.
        _filtered.clear();
        _filtered.shrink_to_fit();
        _query.clear();
        _query.shrink_to_fit();
        _selectedPosition.reset();
        _lastLatestCommandId.reset();
        _firstVisiblePosition = 0;
        _visibleCapacity = 1;
        _wheelDeltaRemainder = 0;
        _open = false;
        _wheelSettlePending = false;
    }

    bool CommandTimelineNavigationModel::IsOpen() const noexcept
    {
        return _open;
    }

    bool CommandTimelineNavigationModel::WheelSettlePending() const noexcept
    {
        return _wheelSettlePending;
    }

    int CommandTimelineNavigationModel::WheelDeltaRemainder() const noexcept
    {
        return _wheelDeltaRemainder;
    }

    size_t CommandTimelineNavigationModel::VisibleCapacity() const noexcept
    {
        return _visibleCapacity;
    }

    const std::wstring& CommandTimelineNavigationModel::Query() const noexcept
    {
        return _query;
    }

    size_t CommandTimelineNavigationModel::FilteredCount() const noexcept
    {
        return _filtered.size();
    }

    void CommandTimelineNavigationModel::_rebuildFilter(const std::span<const CommandTimelineEntry> entries)
    {
        _filtered.clear();
        if (_query.empty())
        {
            _filtered.resize(entries.size());
            for (size_t index = 0; index < entries.size(); ++index)
            {
                _filtered[index] = index;
            }
            return;
        }

        // Only the bounded cached command text is searched. Output is never
        // consulted and the terminal buffer is never rescanned.
        for (size_t index = 0; index < entries.size(); ++index)
        {
            if (CommandTimelineQueryMatches(entries[index].cachedCommandText, _query))
            {
                _filtered.emplace_back(index);
            }
        }
    }

    std::optional<size_t> CommandTimelineNavigationModel::_positionOf(const size_t entryIndex) const noexcept
    {
        const auto found = std::lower_bound(_filtered.begin(), _filtered.end(), entryIndex);
        if (found == _filtered.end() || *found != entryIndex)
        {
            return std::nullopt;
        }
        return gsl::narrow_cast<size_t>(std::distance(_filtered.begin(), found));
    }

    // Chooses the surviving match closest to a command that is no longer in the
    // projection, so a selection is never silently dropped to the bottom.
    size_t CommandTimelineNavigationModel::_nearestPosition(const std::span<const CommandTimelineEntry> entries,
                                                            const CommandId& id) const noexcept
    {
        if (_filtered.empty())
        {
            return 0;
        }
        if (id.paneSessionId != entries[_filtered.front()].id.paneSessionId)
        {
            return _filtered.size() - 1;
        }

        const auto next = std::lower_bound(
            _filtered.begin(), _filtered.end(), id.sequence, [&](const size_t index, const uint64_t sequence) {
                return entries[index].id.sequence < sequence;
            });
        if (next == _filtered.begin())
        {
            return 0;
        }
        if (next == _filtered.end())
        {
            return _filtered.size() - 1;
        }

        const auto nextPosition = gsl::narrow_cast<size_t>(std::distance(_filtered.begin(), next));
        const auto previousPosition = nextPosition - 1;
        const auto nextDistance = entries[*next].id.sequence - id.sequence;
        const auto previousDistance = id.sequence - entries[_filtered[previousPosition]].id.sequence;
        return previousDistance <= nextDistance ? previousPosition : nextPosition;
    }

    void CommandTimelineNavigationModel::_reconcile(const std::span<const CommandTimelineEntry> entries,
                                                    CommandTimelineViewState& viewState,
                                                    const bool allowFollowLatest)
    {
        _rebuildFilter(entries);

        if (_filtered.empty())
        {
            _selectedPosition.reset();
            _lastLatestCommandId = entries.empty() ? std::optional<CommandId>{} : entries.back().id;
            _firstVisiblePosition = 0;
            _syncViewState(entries, viewState);
            return;
        }

        // Following latest only applies when the newest command is itself part
        // of the current projection. A new command that does not match the
        // query must not pull the selection anywhere.
        const auto latestPosition = _filtered.back();
        const auto followingLatest = allowFollowLatest &&
                                     _lastLatestCommandId.has_value() &&
                                     viewState.selectedCommandId == _lastLatestCommandId &&
                                     !entries.empty() &&
                                     entries[latestPosition].id == entries.back().id;

        std::optional<size_t> selected;
        if (followingLatest)
        {
            selected = _filtered.size() - 1;
        }
        else if (viewState.selectedCommandId.has_value())
        {
            if (const auto index = _findCommand(entries, *viewState.selectedCommandId))
            {
                selected = _positionOf(*index);
            }
            if (!selected.has_value())
            {
                selected = _nearestPosition(entries, *viewState.selectedCommandId);
            }
        }
        else
        {
            selected = _filtered.size() - 1;
        }
        _selectedPosition = selected;

        const auto maxFirst = _filtered.size() > _visibleCapacity ? _filtered.size() - _visibleCapacity : 0;
        std::optional<size_t> restoredAnchor;
        if (viewState.visibleNativeAnchor.has_value())
        {
            for (size_t position = 0; position < _filtered.size(); ++position)
            {
                if (entries[_filtered[position]].nativeMarkId == *viewState.visibleNativeAnchor)
                {
                    restoredAnchor = position;
                    break;
                }
            }
        }

        if (restoredAnchor.has_value())
        {
            _firstVisiblePosition = std::min(*restoredAnchor, maxFirst);
        }
        else
        {
            const auto desiredSlot = std::min(viewState.selectedVisualSlot.value_or(_visibleCapacity - 1),
                                              _visibleCapacity - 1);
            _firstVisiblePosition = *_selectedPosition > desiredSlot ? *_selectedPosition - desiredSlot : 0;
            _firstVisiblePosition = std::min(_firstVisiblePosition, maxFirst);
        }

        if (*_selectedPosition < _firstVisiblePosition)
        {
            _firstVisiblePosition = *_selectedPosition;
        }
        else if (*_selectedPosition >= _firstVisiblePosition + _visibleCapacity)
        {
            _firstVisiblePosition = *_selectedPosition - _visibleCapacity + 1;
        }
        _firstVisiblePosition = std::min(_firstVisiblePosition, maxFirst);
        if (!entries.empty())
        {
            _lastLatestCommandId = entries.back().id;
        }
        _syncViewState(entries, viewState);
    }

    bool CommandTimelineNavigationModel::_moveSelection(const NavigationAction action)
    {
        if (!_selectedPosition.has_value() || _filtered.empty())
        {
            return false;
        }

        auto next = *_selectedPosition;
        switch (action)
        {
        case NavigationAction::Previous:
            if (next == 0)
            {
                return false;
            }
            --next;
            break;
        case NavigationAction::Next:
            if (next + 1 >= _filtered.size())
            {
                return false;
            }
            ++next;
            break;
        case NavigationAction::PageFirst:
            next = _firstVisiblePosition;
            break;
        case NavigationAction::PageLast:
            next = std::min(_filtered.size(), _firstVisiblePosition + _visibleCapacity) - 1;
            break;
        }

        _selectedPosition = next;
        if (next < _firstVisiblePosition)
        {
            _firstVisiblePosition = next;
        }
        else if (next >= _firstVisiblePosition + _visibleCapacity)
        {
            _firstVisiblePosition = next - _visibleCapacity + 1;
        }
        return true;
    }

    void CommandTimelineNavigationModel::_syncViewState(const std::span<const CommandTimelineEntry> entries,
                                                        CommandTimelineViewState& viewState) const
    {
        if (!_selectedPosition.has_value() || _filtered.empty() || entries.empty())
        {
            // A query with no results must not clear the selected command; the
            // selection is only dropped when there are no entries at all.
            if (entries.empty())
            {
                viewState.selectedCommandId.reset();
            }
            viewState.visibleNativeAnchor.reset();
            viewState.selectedVisualSlot.reset();
            return;
        }

        viewState.selectedCommandId = entries[_filtered[*_selectedPosition]].id;
        viewState.visibleNativeAnchor = entries[_filtered[_firstVisiblePosition]].nativeMarkId;
        viewState.selectedVisualSlot = *_selectedPosition - _firstVisiblePosition;
    }

    CommandTimelinePresentationSnapshot CommandTimelineNavigationModel::_snapshot(
        const std::span<const CommandTimelineEntry> entries,
        const ShellIntegrationCapability capability) const
    {
        const auto emptyState = [&]() noexcept {
            if (!_filtered.empty())
            {
                return CommandTimelineEmptyState::None;
            }
            if (!_query.empty() && !entries.empty())
            {
                return CommandTimelineEmptyState::NoMatchingCommands;
            }
            switch (capability)
            {
            case ShellIntegrationCapability::Limited:
                return CommandTimelineEmptyState::ShellUnsupported;
            case ShellIntegrationCapability::Full:
                return CommandTimelineEmptyState::NoCommands;
            default:
                return CommandTimelineEmptyState::WaitingForShell;
            }
        }();

        CommandTimelinePresentationSnapshot result{
            .capability = capability,
            .emptyState = emptyState,
            .totalEntryCount = entries.size(),
            .filteredEntryCount = _filtered.size(),
            .firstVisibleIndex = _firstVisiblePosition,
            .selectedVisualSlot = _selectedPosition.has_value() ? *_selectedPosition - _firstVisiblePosition : 0,
            .wheelDeltaRemainder = _wheelDeltaRemainder,
            .open = _open,
            .filtered = !_query.empty(),
            .wheelSettlePending = _wheelSettlePending,
        };
        if (!_open || _filtered.empty())
        {
            return result;
        }

        // Only the rows that fit on screen are materialized, whatever the size
        // of the history behind them.
        const auto end = std::min(_filtered.size(), _firstVisiblePosition + _visibleCapacity);
        result.visibleEntries.reserve(end - _firstVisiblePosition);
        for (auto position = _firstVisiblePosition; position < end; ++position)
        {
            const auto& entry = entries[_filtered[position]];
            result.visibleEntries.emplace_back(CommandTimelineVisibleEntry{
                .id = entry.id,
                .commandText = entry.cachedCommandText,
                .executionResult = _effectiveResult(entry),
                .selected = _selectedPosition == position,
            });
        }
        return result;
    }

    std::optional<size_t> CommandTimelineNavigationModel::_findCommand(
        const std::span<const CommandTimelineEntry> entries,
        const CommandId& id) noexcept
    {
        const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
            return entry.id == id;
        });
        if (found == entries.end())
        {
            return std::nullopt;
        }
        return gsl::narrow_cast<size_t>(std::distance(entries.begin(), found));
    }

    size_t CommandTimelineNavigationModel::_findNearestCommand(
        const std::span<const CommandTimelineEntry> entries,
        const CommandId& id) noexcept
    {
        if (id.paneSessionId != entries.front().id.paneSessionId)
        {
            return entries.size() - 1;
        }
        const auto next = std::lower_bound(entries.begin(), entries.end(), id.sequence, [](const auto& entry, const auto sequence) {
            return entry.id.sequence < sequence;
        });
        if (next == entries.begin())
        {
            return 0;
        }
        if (next == entries.end())
        {
            return entries.size() - 1;
        }

        const auto nextIndex = gsl::narrow_cast<size_t>(std::distance(entries.begin(), next));
        const auto previousIndex = nextIndex - 1;
        const auto nextDistance = next->id.sequence - id.sequence;
        const auto previousDistance = id.sequence - entries[previousIndex].id.sequence;
        return previousDistance <= nextDistance ? previousIndex : nextIndex;
    }

    ExecutionResult CommandTimelineNavigationModel::_effectiveResult(const CommandTimelineEntry& entry) noexcept
    {
        if (entry.shellIntegrationCapability != ShellIntegrationCapability::Full &&
            (entry.executionResult == ExecutionResult::Succeeded || entry.executionResult == ExecutionResult::Failed))
        {
            return ExecutionResult::Unknown;
        }
        return entry.executionResult;
    }

    bool CommandActionRequest::Actionable() const noexcept
    {
        return status == CommandActionStatus::Ready;
    }

    bool CommandTimelineActionModel::IsMultiline(const std::wstring_view commandText) noexcept
    {
        return commandText.find_first_of(L"\r\n") != std::wstring_view::npos;
    }

    const CommandTimelineEntry* CommandTimelineActionModel::_selected(
        const std::span<const CommandTimelineEntry> entries,
        const CommandTimelineViewState& viewState) noexcept
    {
        if (!viewState.selectedCommandId.has_value())
        {
            return nullptr;
        }

        const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
            return entry.id == *viewState.selectedCommandId;
        });
        return found == entries.end() ? nullptr : &*found;
    }

    CommandActionRequest CommandTimelineActionModel::Prepare(const CommandActionKind kind,
                                                             const std::span<const CommandTimelineEntry> entries,
                                                             const CommandTimelineViewState& viewState,
                                                             const CommandLoadPolicy& policy) const
    {
        CommandActionRequest request{
            .kind = kind,
            .status = CommandActionStatus::NoSelection,
            .executionGeneration = viewState.executionGeneration,
        };

        const auto* const entry = _selected(entries, viewState);
        if (!entry)
        {
            return request;
        }

        request.id = entry->id;
        request.nativeMarkId = entry->nativeMarkId;
        request.commandText = entry->cachedCommandText;
        request.characterCount = entry->cachedCommandText.size();
        request.multiline = IsMultiline(entry->cachedCommandText);

        switch (kind)
        {
        case CommandActionKind::JumpToOutput:
            // Jumping only needs a native mark that still resolves to a live
            // range. The output text itself stays in the terminal buffer.
            request.status = entry->nativeMarkId != 0 && entry->nativeRangeValid ?
                                 CommandActionStatus::Ready :
                                 CommandActionStatus::OutputUnavailable;
            return request;
        case CommandActionKind::CopyOutput:
            // Output is resolved by the caller on demand. The Timeline holds no
            // output cache, so this only reports whether a resolvable range
            // still exists.
            request.status = entry->nativeMarkId != 0 && entry->nativeRangeValid &&
                                     entry->lifecycleState != CommandLifecycleState::Command ?
                                 CommandActionStatus::Ready :
                                 CommandActionStatus::OutputUnavailable;
            return request;
        case CommandActionKind::CopyCommand:
            request.status = entry->cachedCommandText.empty() ?
                                 CommandActionStatus::CommandTextUnavailable :
                                 CommandActionStatus::Ready;
            return request;
        case CommandActionKind::LoadIntoInput:
            break;
        }

        if (entry->cachedCommandText.empty())
        {
            request.status = CommandActionStatus::CommandTextUnavailable;
            return request;
        }

        // A multiline command can only be placed on the input line safely when
        // the shell brackets the paste. Without bracketed paste the embedded
        // line breaks would be consumed as command submissions, which would
        // execute the command the Timeline is only supposed to load.
        if (request.multiline && !policy.bracketedPasteEnabled)
        {
            request.status = CommandActionStatus::MultilineUnsafe;
            return request;
        }

        const auto warnMultiline = policy.warnOnMultilineLoad && request.multiline;
        const auto warnLarge = policy.warnOnLargeLoad &&
                               request.characterCount > policy.largeLoadCharacterThreshold;
        request.status = warnMultiline || warnLarge ?
                             CommandActionStatus::ConfirmationRequired :
                             CommandActionStatus::Ready;
        return request;
    }

    CommandActionRequest CommandTimelineActionModel::Confirm(CommandActionRequest request) noexcept
    {
        if (request.status == CommandActionStatus::ConfirmationRequired)
        {
            request.status = CommandActionStatus::Ready;
        }
        return request;
    }

    void CommandTimelineActionModel::NotifyLoaded(CommandTimelineViewState& viewState,
                                                  const CommandActionRequest& request) noexcept
    {
        if (request.kind != CommandActionKind::LoadIntoInput || !request.Actionable())
        {
            return;
        }

        viewState.loadedCommandId = request.id;
        viewState.loadedIntoInput = true;
        ++viewState.executionGeneration;
    }

    void CommandTimelineActionModel::NotifyExecutionStarted(CommandTimelineViewState& viewState) noexcept
    {
        // The pane started a new command, so any text the Timeline previously
        // placed on the input line has been consumed. Retiring the generation
        // here is what makes a late completion from the previous command
        // detectable by IsCurrentGeneration.
        viewState.loadedCommandId.reset();
        viewState.loadedIntoInput = false;
        ++viewState.executionGeneration;
    }

    void CommandTimelineActionModel::ReconcileLoadedInput(const std::span<const CommandTimelineEntry> entries,
                                                          CommandTimelineViewState& viewState) noexcept
    {
        if (!viewState.loadedCommandId.has_value())
        {
            viewState.loadedIntoInput = false;
            return;
        }

        const auto survives = std::any_of(entries.begin(), entries.end(), [&](const auto& entry) {
            return entry.id == *viewState.loadedCommandId;
        });
        if (!survives)
        {
            viewState.loadedCommandId.reset();
            viewState.loadedIntoInput = false;
        }
    }

    bool CommandTimelineActionModel::IsCurrentGeneration(const CommandTimelineViewState& viewState,
                                                         const uint64_t executionGeneration) noexcept
    {
        return viewState.executionGeneration == executionGeneration;
    }

    void CommandTimelineActionModel::Reset(CommandTimelineViewState& viewState) noexcept
    {
        viewState.loadedCommandId.reset();
        viewState.loadedIntoInput = false;
    }

    std::wstring CommandTextCachePolicy::Apply(const std::wstring_view commandText) const
    {
        if (commandText.size() <= maxCommandTextLength)
        {
            return std::wstring{ commandText };
        }

        std::wstring result{ commandText.substr(0, maxCommandTextLength) };
        if (!result.empty() && IsHighSurrogate(result.back()) &&
            commandText.size() > result.size() && IsLowSurrogate(commandText[result.size()]))
        {
            result.pop_back();
        }
        return result;
    }

    CommandTimelineIndex::CommandTimelineIndex(const winrt::guid paneSessionId,
                                               const CommandTextCachePolicy cachePolicy) :
        _paneSessionId{ paneSessionId },
        _cachePolicy{ cachePolicy }
    {
    }

    const std::vector<CommandTimelineEntry>& CommandTimelineIndex::Access(const uint64_t nativeRevision,
                                                                          const BootstrapLoader& loader)
    {
        if (_closed)
        {
            return _entries;
        }

        if (!_bootstrapped || nativeRevision != _nativeRevision)
        {
            ++_bootstrapScanCount;
            _applyBootstrap(loader());
        }
        return _entries;
    }

    void CommandTimelineIndex::ProcessLifecycle(const LifecycleUpdate& update)
    {
        if (_closed || update.nativeMarkId == 0)
        {
            return;
        }

        _nativeRevision = update.nativeRevision;
        const auto priorLifecycle = _lifecycleByNativeMark[update.nativeMarkId];
        _observeLifecycleCapability(update.nativeMarkId, update.kind);

        const auto existing = _byNativeMark.find(update.nativeMarkId);
        CommandTimelineEntry* entry = existing == _byNativeMark.end() ? nullptr : &_entries[existing->second];
        bool changed = false;

        switch (update.kind)
        {
        case LifecycleEventKind::Prompt:
            if (_currentSequence.has_value())
            {
                const auto current = _bySequence.find(*_currentSequence);
                if (current != _bySequence.end() && _entries[current->second].nativeMarkId != update.nativeMarkId)
                {
                    _finishIncompleteCurrent(update.timestamp);
                    changed = true;
                }
            }
            break;
        case LifecycleEventKind::CommandStart:
            if (_currentSequence.has_value() && (!entry || *_currentSequence != entry->id.sequence))
            {
                _finishIncompleteCurrent(update.timestamp);
                changed = true;
            }
            if (!entry)
            {
                entry = &_createEntry(update.nativeMarkId, update.timestamp);
                changed = true;
            }
            _currentSequence = entry->id.sequence;
            if (entry->lifecycleState != CommandLifecycleState::Command ||
                entry->executionResult != ExecutionResult::Running)
            {
                entry->lifecycleState = CommandLifecycleState::Command;
                entry->executionResult = ExecutionResult::Running;
                entry->trustedExitCode.reset();
                entry->endTimestamp.reset();
                changed = true;
            }
            break;
        case LifecycleEventKind::OutputStart:
        {
            const auto missingCommandStart = (priorLifecycle & CommandStartEvent) == 0;
            if (!entry)
            {
                entry = &_createEntry(update.nativeMarkId, update.timestamp);
                changed = true;
            }
            _currentSequence = entry->id.sequence;
            {
                const auto cached = _cachePolicy.Apply(update.commandText);
                if (entry->cachedCommandText != cached)
                {
                    entry->cachedCommandText = cached;
                    changed = true;
                }
            }
            const auto result = missingCommandStart ? ExecutionResult::Unknown : ExecutionResult::Running;
            if (entry->lifecycleState != CommandLifecycleState::Output ||
                entry->executionResult != result)
            {
                entry->lifecycleState = CommandLifecycleState::Output;
                entry->executionResult = result;
                changed = true;
            }
            break;
        }
        case LifecycleEventKind::CommandFinished:
        case LifecycleEventKind::CommandCancelled:
        {
            const auto lifecycleComplete = (priorLifecycle & (CommandStartEvent | OutputStartEvent)) ==
                                           (CommandStartEvent | OutputStartEvent);
            if (!entry)
            {
                entry = &_createEntry(update.nativeMarkId, update.timestamp);
                entry->executionResult = ExecutionResult::Unknown;
                changed = true;
            }
            if (!update.commandText.empty())
            {
                const auto cached = _cachePolicy.Apply(update.commandText);
                if (entry->cachedCommandText != cached)
                {
                    entry->cachedCommandText = cached;
                    changed = true;
                }
            }
            const auto finalState = update.kind == LifecycleEventKind::CommandCancelled || lifecycleComplete ?
                                        CommandLifecycleState::Completed :
                                        CommandLifecycleState::Incomplete;
            if (entry->lifecycleState != finalState)
            {
                entry->lifecycleState = finalState;
                changed = true;
            }
            if (update.kind == LifecycleEventKind::CommandCancelled)
            {
                if (entry->executionResult != ExecutionResult::Cancelled)
                {
                    entry->executionResult = ExecutionResult::Cancelled;
                    changed = true;
                }
                entry->trustedExitCode.reset();
            }
            else if (lifecycleComplete && update.trustedExitCode.has_value())
            {
                const auto result = *update.trustedExitCode == 0 ? ExecutionResult::Succeeded : ExecutionResult::Failed;
                if (entry->executionResult != result || entry->trustedExitCode != update.trustedExitCode)
                {
                    entry->executionResult = result;
                    entry->trustedExitCode = update.trustedExitCode;
                    changed = true;
                }
            }
            else if (entry->executionResult != ExecutionResult::Unknown || entry->trustedExitCode.has_value())
            {
                entry->executionResult = ExecutionResult::Unknown;
                entry->trustedExitCode.reset();
                changed = true;
            }
            if (entry->endTimestamp != update.timestamp)
            {
                entry->endTimestamp = update.timestamp;
                changed = true;
            }
            if (_currentSequence == entry->id.sequence)
            {
                _currentSequence.reset();
            }
            break;
        }
        }

        if (entry && entry->shellIntegrationCapability != _capability)
        {
            entry->shellIntegrationCapability = _capability;
            changed = true;
        }

        if (changed)
        {
            _incrementRevision();
        }
    }

    void CommandTimelineIndex::InvalidateNativeMarks(const std::span<const uint64_t> nativeMarkIds,
                                                     const uint64_t nativeRevision)
    {
        if (_closed)
        {
            return;
        }

        _nativeRevision = nativeRevision;
        if (nativeMarkIds.empty())
        {
            return;
        }

        const std::unordered_set<uint64_t> invalid{ nativeMarkIds.begin(), nativeMarkIds.end() };
        const auto oldSize = _entries.size();
        std::erase_if(_entries, [&](const auto& entry) {
            return invalid.contains(entry.nativeMarkId);
        });
        if (_entries.size() != oldSize)
        {
            _rebuildLookups();
            _incrementRevision();
        }
        for (const auto nativeMarkId : nativeMarkIds)
        {
            _lifecycleByNativeMark.erase(nativeMarkId);
        }
    }

    void CommandTimelineIndex::ReconcileReflow(const std::span<const uint64_t> survivingNativeMarkIds,
                                               const uint64_t nativeRevision)
    {
        if (_closed)
        {
            return;
        }

        _nativeRevision = nativeRevision;
        const std::unordered_set<uint64_t> surviving{ survivingNativeMarkIds.begin(), survivingNativeMarkIds.end() };
        const auto oldSize = _entries.size();
        std::erase_if(_entries, [&](const auto& entry) {
            return !surviving.contains(entry.nativeMarkId);
        });
        if (_entries.size() != oldSize)
        {
            _rebuildLookups();
            _incrementRevision();
        }
        std::erase_if(_lifecycleByNativeMark, [&](const auto& lifecycle) {
            return !surviving.contains(lifecycle.first);
        });
    }

    void CommandTimelineIndex::SetCapabilityFallback(const ShellIntegrationCapability capability)
    {
        if (_closed || capability == ShellIntegrationCapability::Full || _capability == ShellIntegrationCapability::Full)
        {
            return;
        }
        if (_capability != capability)
        {
            _capability = capability;
            _refreshEntryCapabilities();
            _incrementRevision();
        }
    }

    void CommandTimelineIndex::SetHistoryLimit(const size_t historyLimit)
    {
        const auto clamped = ClampCommandTimelineHistoryLimit(gsl::narrow_cast<int64_t>(historyLimit));
        if (_closed || _historyLimit == clamped)
        {
            return;
        }

        _historyLimit = clamped;
        // Raising the limit must not resurrect anything: entries already
        // dropped are gone, and sequence numbers are never reissued.
        if (_applyHistoryLimit())
        {
            _rebuildLookups();
            _incrementRevision();
        }
    }

    // Drops the oldest entries first until the history fits the limit.
    bool CommandTimelineIndex::_applyHistoryLimit()
    {
        if (_entries.size() <= _historyLimit)
        {
            return false;
        }

        const auto excess = _entries.size() - _historyLimit;
        for (size_t index = 0; index < excess; ++index)
        {
            _lifecycleByNativeMark.erase(_entries[index].nativeMarkId);
        }
        _entries.erase(_entries.begin(), _entries.begin() + gsl::narrow_cast<ptrdiff_t>(excess));
        return true;
    }

    size_t CommandTimelineIndex::HistoryLimit() const noexcept
    {
        return _historyLimit;
    }

    void CommandTimelineIndex::Close() noexcept
    {
        if (_closed)
        {
            return;
        }
        _closed = true;
        _entries.clear();
        _byNativeMark.clear();
        _bySequence.clear();
        _lifecycleByNativeMark.clear();
        _currentSequence.reset();
        _capability = ShellIntegrationCapability::Unknown;
        _bootstrapped = false;
        _nativeRevision = 0;
        _incrementRevision();
    }

    const CommandTimelineEntry* CommandTimelineIndex::Find(const CommandId& id) const noexcept
    {
        if (id.paneSessionId != _paneSessionId)
        {
            return nullptr;
        }
        if (const auto found = _bySequence.find(id.sequence); found != _bySequence.end())
        {
            return &_entries[found->second];
        }
        return nullptr;
    }

    const CommandTimelineEntry* CommandTimelineIndex::Current() const noexcept
    {
        if (_currentSequence.has_value())
        {
            if (const auto found = _bySequence.find(*_currentSequence); found != _bySequence.end())
            {
                return &_entries[found->second];
            }
        }
        return nullptr;
    }

    const std::vector<CommandTimelineEntry>& CommandTimelineIndex::Entries() const noexcept
    {
        return _entries;
    }

    const winrt::guid& CommandTimelineIndex::PaneSessionId() const noexcept
    {
        return _paneSessionId;
    }

    ShellIntegrationCapability CommandTimelineIndex::Capability() const noexcept
    {
        return _capability;
    }

    uint64_t CommandTimelineIndex::Revision() const noexcept
    {
        return _revision;
    }

    uint64_t CommandTimelineIndex::NativeRevision() const noexcept
    {
        return _nativeRevision;
    }

    uint64_t CommandTimelineIndex::NextSequence() const noexcept
    {
        return _nextSequence;
    }

    size_t CommandTimelineIndex::BootstrapScanCount() const noexcept
    {
        return _bootstrapScanCount;
    }

    size_t CommandTimelineIndex::CachedCommandTextCharacters() const noexcept
    {
        size_t total = 0;
        for (const auto& entry : _entries)
        {
            total += entry.cachedCommandText.size();
        }
        return total;
    }

    bool CommandTimelineIndex::IsBootstrapped() const noexcept
    {
        return _bootstrapped;
    }

    bool CommandTimelineIndex::IsClosed() const noexcept
    {
        return _closed;
    }

    CommandTimelineEntry& CommandTimelineIndex::_createEntry(const uint64_t nativeMarkId,
                                                             const std::optional<std::chrono::system_clock::time_point> timestamp)
    {
        const auto sequence = _nextSequence++;
        _entries.emplace_back(CommandTimelineEntry{
            .id = { _paneSessionId, sequence },
            .nativeMarkId = nativeMarkId,
            .lifecycleState = CommandLifecycleState::Command,
            .executionResult = ExecutionResult::Running,
            .startTimestamp = timestamp,
            .nativeRangeValid = true,
            .shellIntegrationCapability = _capability,
        });
        // The newest command is always kept; eviction takes from the front.
        _applyHistoryLimit();
        _rebuildLookups();
        return _entries.back();
    }

    void CommandTimelineIndex::_applyBootstrap(NativeBootstrapSnapshot snapshot)
    {
        const std::unordered_set<uint64_t> validMarks = [&]() {
            std::unordered_set<uint64_t> result;
            for (const auto& mark : snapshot.marks)
            {
                if (mark.nativeMarkId != 0 && mark.nativeRangeValid)
                {
                    result.emplace(mark.nativeMarkId);
                }
            }
            return result;
        }();

        bool changed = !_bootstrapped;
        if (_bootstrapped)
        {
            const auto oldSize = _entries.size();
            std::erase_if(_entries, [&](const auto& entry) {
                return !validMarks.contains(entry.nativeMarkId);
            });
            changed = _entries.size() != oldSize;
            _rebuildLookups();
            std::erase_if(_lifecycleByNativeMark, [&](const auto& lifecycle) {
                return !validMarks.contains(lifecycle.first);
            });
        }

        for (const auto& mark : snapshot.marks)
        {
            if (mark.nativeMarkId == 0 || !mark.nativeRangeValid)
            {
                continue;
            }

            const auto found = _byNativeMark.find(mark.nativeMarkId);
            CommandTimelineEntry* entry = found == _byNativeMark.end() ? nullptr : &_entries[found->second];
            if (!entry && mark.hasCommand)
            {
                entry = &_createEntry(mark.nativeMarkId, std::nullopt);
                changed = true;
            }
            if (!entry)
            {
                continue;
            }

            auto lifecycle = PromptEvent;
            if (mark.hasCommand)
            {
                lifecycle |= CommandStartEvent;
            }
            if (mark.hasOutput)
            {
                lifecycle |= OutputStartEvent;
            }
            if (mark.trustedExitCode.has_value())
            {
                lifecycle |= CommandFinishedEvent;
            }
            _lifecycleByNativeMark[mark.nativeMarkId] = lifecycle;

            const auto cached = _cachePolicy.Apply(mark.commandText);
            if (entry->cachedCommandText != cached)
            {
                entry->cachedCommandText = cached;
                changed = true;
            }
            if (entry->nativeRangeValid != mark.nativeRangeValid)
            {
                entry->nativeRangeValid = mark.nativeRangeValid;
                changed = true;
            }
            if (mark.trustedExitCode.has_value())
            {
                const auto result = *mark.trustedExitCode == 0 ? ExecutionResult::Succeeded : ExecutionResult::Failed;
                if (entry->lifecycleState != CommandLifecycleState::Completed ||
                    entry->executionResult != result || entry->trustedExitCode != mark.trustedExitCode)
                {
                    entry->lifecycleState = CommandLifecycleState::Completed;
                    entry->executionResult = result;
                    entry->trustedExitCode = mark.trustedExitCode;
                    changed = true;
                }
            }
            else
            {
                const auto state = mark.hasOutput ? CommandLifecycleState::Output : CommandLifecycleState::Command;
                if (entry->lifecycleState != state || entry->executionResult != ExecutionResult::Running)
                {
                    entry->lifecycleState = state;
                    entry->executionResult = ExecutionResult::Running;
                    entry->trustedExitCode.reset();
                    changed = true;
                }
            }
        }

        _nativeRevision = snapshot.nativeRevision;
        _bootstrapped = true;
        changed = _applyHistoryLimit() || changed;
        _rebuildLookups();
        if (changed)
        {
            _incrementRevision();
        }
    }

    void CommandTimelineIndex::_finishIncompleteCurrent(const std::optional<std::chrono::system_clock::time_point> timestamp)
    {
        if (!_currentSequence.has_value())
        {
            return;
        }
        if (const auto current = _bySequence.find(*_currentSequence); current != _bySequence.end())
        {
            auto& entry = _entries[current->second];
            entry.lifecycleState = CommandLifecycleState::Incomplete;
            entry.executionResult = ExecutionResult::Unknown;
            entry.trustedExitCode.reset();
            entry.endTimestamp = timestamp;
        }
        _currentSequence.reset();
    }

    void CommandTimelineIndex::_observeLifecycleCapability(const uint64_t nativeMarkId,
                                                           const LifecycleEventKind kind)
    {
        auto& observed = _lifecycleByNativeMark[nativeMarkId];
        switch (kind)
        {
        case LifecycleEventKind::Prompt:
            if (observed != PromptEvent)
            {
                observed = PromptEvent;
            }
            break;
        case LifecycleEventKind::CommandStart:
            observed = (observed & PromptEvent) != 0 ?
                           static_cast<uint8_t>(PromptEvent | CommandStartEvent) :
                           CommandStartEvent;
            break;
        case LifecycleEventKind::OutputStart:
            observed = (observed & CommandStartEvent) != 0 ?
                           static_cast<uint8_t>(observed | OutputStartEvent) :
                           OutputStartEvent;
            break;
        case LifecycleEventKind::CommandFinished:
            if ((observed & (PromptEvent | CommandStartEvent | OutputStartEvent)) ==
                (PromptEvent | CommandStartEvent | OutputStartEvent))
            {
                observed |= CommandFinishedEvent;
            }
            break;
        case LifecycleEventKind::CommandCancelled:
            break;
        }

        if ((observed & FullLifecycle) == FullLifecycle &&
            _capability != ShellIntegrationCapability::Full)
        {
            _capability = ShellIntegrationCapability::Full;
            _refreshEntryCapabilities();
        }
    }

    void CommandTimelineIndex::_refreshEntryCapabilities()
    {
        for (auto& entry : _entries)
        {
            entry.shellIntegrationCapability = _capability;
        }
    }

    void CommandTimelineIndex::_rebuildLookups()
    {
        _byNativeMark.clear();
        _bySequence.clear();
        for (size_t index = 0; index < _entries.size(); ++index)
        {
            _byNativeMark.emplace(_entries[index].nativeMarkId, index);
            _bySequence.emplace(_entries[index].id.sequence, index);
        }
        if (_currentSequence.has_value() && !_bySequence.contains(*_currentSequence))
        {
            _currentSequence.reset();
        }
    }

    void CommandTimelineIndex::_incrementRevision() noexcept
    {
        ++_revision;
    }

    ShellIntegrationCapability InferCapabilityFallbackFromCommandline(const std::wstring_view commandline) noexcept
    {
        auto firstToken = commandline;
        const auto firstNonSpace = firstToken.find_first_not_of(L" \t");
        if (firstNonSpace == std::wstring_view::npos)
        {
            return ShellIntegrationCapability::Unknown;
        }
        firstToken.remove_prefix(firstNonSpace);

        if (firstToken.front() == L'\"')
        {
            firstToken.remove_prefix(1);
            if (const auto quote = firstToken.find(L'\"'); quote != std::wstring_view::npos)
            {
                firstToken = firstToken.substr(0, quote);
            }
        }
        else if (const auto whitespace = firstToken.find_first_of(L" \t"); whitespace != std::wstring_view::npos)
        {
            firstToken = firstToken.substr(0, whitespace);
        }

        if (const auto separator = firstToken.find_last_of(L"\\/"); separator != std::wstring_view::npos)
        {
            firstToken.remove_prefix(separator + 1);
        }

        std::wstring executable{ firstToken };
        std::transform(executable.begin(), executable.end(), executable.begin(), [](const wchar_t value) {
            return static_cast<wchar_t>(std::towlower(value));
        });
        return executable == L"cmd" || executable == L"cmd.exe" ? ShellIntegrationCapability::Limited :
                                                                  ShellIntegrationCapability::Unknown;
    }
}
