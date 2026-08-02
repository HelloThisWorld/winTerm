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

    void CommandTimelineViewState::Reset() noexcept
    {
        selectedCommandId.reset();
        visibleNativeAnchor.reset();
        selectedVisualSlot.reset();
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
            _moveSelection(action, entries);
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
            const auto index = _firstVisibleIndex + visualSlot;
            if (visualSlot < _visibleCapacity && index < entries.size())
            {
                _selectedIndex = index;
                _syncViewState(entries, viewState);
            }
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
        _wheelSettlePending = true;
        const auto accumulated = static_cast<int64_t>(_wheelDeltaRemainder) + delta;
        _wheelDeltaRemainder = gsl::narrow_cast<int>(std::clamp<int64_t>(
            accumulated,
            std::numeric_limits<int>::min(),
            std::numeric_limits<int>::max()));

        while (_wheelDeltaRemainder >= deltaPerEntry)
        {
            _wheelDeltaRemainder -= deltaPerEntry;
            if (!_moveSelection(NavigationAction::Previous, entries))
            {
                _wheelDeltaRemainder = 0;
                break;
            }
        }
        while (_wheelDeltaRemainder <= -deltaPerEntry)
        {
            _wheelDeltaRemainder += deltaPerEntry;
            if (!_moveSelection(NavigationAction::Next, entries))
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
        _selectedIndex.reset();
        _lastLatestCommandId.reset();
        _firstVisibleIndex = 0;
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

    void CommandTimelineNavigationModel::_reconcile(const std::span<const CommandTimelineEntry> entries,
                                                    CommandTimelineViewState& viewState,
                                                    const bool allowFollowLatest)
    {
        if (entries.empty())
        {
            _selectedIndex.reset();
            _lastLatestCommandId.reset();
            _firstVisibleIndex = 0;
            _syncViewState(entries, viewState);
            return;
        }

        const auto wasFollowingLatest = allowFollowLatest &&
                                        _lastLatestCommandId.has_value() &&
                                        viewState.selectedCommandId == _lastLatestCommandId;
        std::optional<size_t> selected;
        if (wasFollowingLatest)
        {
            selected = entries.size() - 1;
        }
        else if (viewState.selectedCommandId.has_value())
        {
            selected = _findCommand(entries, *viewState.selectedCommandId);
            if (!selected.has_value())
            {
                selected = _findNearestCommand(entries, *viewState.selectedCommandId);
            }
        }
        else
        {
            selected = entries.size() - 1;
        }
        _selectedIndex = selected;

        const auto maxFirst = entries.size() > _visibleCapacity ? entries.size() - _visibleCapacity : 0;
        std::optional<size_t> restoredAnchor;
        if (viewState.visibleNativeAnchor.has_value())
        {
            const auto anchor = std::find_if(entries.begin(), entries.end(), [&](const auto& entry) {
                return entry.nativeMarkId == *viewState.visibleNativeAnchor;
            });
            if (anchor != entries.end())
            {
                restoredAnchor = gsl::narrow_cast<size_t>(std::distance(entries.begin(), anchor));
            }
        }

        if (restoredAnchor.has_value())
        {
            _firstVisibleIndex = std::min(*restoredAnchor, maxFirst);
        }
        else
        {
            const auto desiredSlot = std::min(viewState.selectedVisualSlot.value_or(_visibleCapacity - 1),
                                              _visibleCapacity - 1);
            _firstVisibleIndex = *_selectedIndex > desiredSlot ? *_selectedIndex - desiredSlot : 0;
            _firstVisibleIndex = std::min(_firstVisibleIndex, maxFirst);
        }

        if (*_selectedIndex < _firstVisibleIndex)
        {
            _firstVisibleIndex = *_selectedIndex;
        }
        else if (*_selectedIndex >= _firstVisibleIndex + _visibleCapacity)
        {
            _firstVisibleIndex = *_selectedIndex - _visibleCapacity + 1;
        }
        _firstVisibleIndex = std::min(_firstVisibleIndex, maxFirst);
        _lastLatestCommandId = entries.back().id;
        _syncViewState(entries, viewState);
    }

    bool CommandTimelineNavigationModel::_moveSelection(const NavigationAction action,
                                                        const std::span<const CommandTimelineEntry> entries)
    {
        if (!_selectedIndex.has_value() || entries.empty())
        {
            return false;
        }

        auto next = *_selectedIndex;
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
            if (next + 1 >= entries.size())
            {
                return false;
            }
            ++next;
            break;
        case NavigationAction::PageFirst:
            next = _firstVisibleIndex;
            break;
        case NavigationAction::PageLast:
            next = std::min(entries.size(), _firstVisibleIndex + _visibleCapacity) - 1;
            break;
        }

        _selectedIndex = next;
        if (next < _firstVisibleIndex)
        {
            _firstVisibleIndex = next;
        }
        else if (next >= _firstVisibleIndex + _visibleCapacity)
        {
            _firstVisibleIndex = next - _visibleCapacity + 1;
        }
        return true;
    }

    void CommandTimelineNavigationModel::_syncViewState(const std::span<const CommandTimelineEntry> entries,
                                                        CommandTimelineViewState& viewState) const
    {
        if (!_selectedIndex.has_value() || entries.empty())
        {
            viewState.selectedCommandId.reset();
            viewState.visibleNativeAnchor.reset();
            viewState.selectedVisualSlot.reset();
            return;
        }

        viewState.selectedCommandId = entries[*_selectedIndex].id;
        viewState.visibleNativeAnchor = entries[_firstVisibleIndex].nativeMarkId;
        viewState.selectedVisualSlot = *_selectedIndex - _firstVisibleIndex;
    }

    CommandTimelinePresentationSnapshot CommandTimelineNavigationModel::_snapshot(
        const std::span<const CommandTimelineEntry> entries,
        const ShellIntegrationCapability capability) const
    {
        CommandTimelinePresentationSnapshot result{
            .capability = capability,
            .totalEntryCount = entries.size(),
            .firstVisibleIndex = _firstVisibleIndex,
            .selectedVisualSlot = _selectedIndex.has_value() ? *_selectedIndex - _firstVisibleIndex : 0,
            .wheelDeltaRemainder = _wheelDeltaRemainder,
            .open = _open,
            .wheelSettlePending = _wheelSettlePending,
        };
        if (!_open || entries.empty())
        {
            return result;
        }

        const auto end = std::min(entries.size(), _firstVisibleIndex + _visibleCapacity);
        result.visibleEntries.reserve(end - _firstVisibleIndex);
        for (auto index = _firstVisibleIndex; index < end; ++index)
        {
            const auto& entry = entries[index];
            result.visibleEntries.emplace_back(CommandTimelineVisibleEntry{
                .id = entry.id,
                .commandText = entry.cachedCommandText,
                .executionResult = _effectiveResult(entry),
                .selected = _selectedIndex == index,
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
