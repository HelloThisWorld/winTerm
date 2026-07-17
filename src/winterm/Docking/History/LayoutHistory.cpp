// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "LayoutHistory.h"

#include "../Layout/LayoutTree.h"

#include <algorithm>

using namespace winTerm::Docking;

bool LayoutHistoryResult::Succeeded() const noexcept
{
    return status == LayoutHistoryStatus::Applied;
}

LayoutHistory::LayoutHistory(const size_t limit) :
    _limit{ std::clamp<size_t>(limit, 1, 100) }
{
}

void LayoutHistory::Limit(const size_t limit)
{
    _limit = std::clamp<size_t>(limit, 1, 100);
    _trim();
}

size_t LayoutHistory::Limit() const noexcept
{
    return _limit;
}

size_t LayoutHistory::UndoCount() const noexcept
{
    return _undo.size();
}

size_t LayoutHistory::RedoCount() const noexcept
{
    return _redo.size();
}

bool LayoutHistory::Record(LayoutHistoryEntry entry)
{
    if (entry.transactionId.empty() || entry.sessionIds.empty())
    {
        return false;
    }
    entry.sequence = _nextSequence++;
    _undo.emplace_back(_clone(entry));
    _redo.clear();
    _trim();
    return true;
}

bool LayoutHistory::CanUndo(const SessionsAvailableCallback& sessionsAvailable) const
{
    return !_undo.empty() && _available(_undo.back(), sessionsAvailable);
}

bool LayoutHistory::CanRedo(const SessionsAvailableCallback& sessionsAvailable) const
{
    return !_redo.empty() && _available(_redo.back(), sessionsAvailable);
}

LayoutHistoryResult LayoutHistory::Undo(
    const SessionsAvailableCallback& sessionsAvailable,
    const ApplyCallback& apply)
{
    if (_undo.empty())
    {
        return { LayoutHistoryStatus::Empty, "There is no layout change to undo." };
    }
    const auto& candidate = _undo.back();
    if (!_available(candidate, sessionsAvailable))
    {
        return { LayoutHistoryStatus::SessionUnavailable, "A session required by this layout change is no longer available." };
    }
    if (!apply || !apply(candidate, true))
    {
        return { LayoutHistoryStatus::ApplyFailed, "The layout change could not be undone safely." };
    }
    auto entry = std::move(_undo.back());
    _undo.pop_back();
    _redo.emplace_back(_clone(entry));
    return { LayoutHistoryStatus::Applied, "The layout change was undone.", std::move(entry) };
}

LayoutHistoryResult LayoutHistory::Redo(
    const SessionsAvailableCallback& sessionsAvailable,
    const ApplyCallback& apply)
{
    if (_redo.empty())
    {
        return { LayoutHistoryStatus::Empty, "There is no layout change to redo." };
    }
    const auto& candidate = _redo.back();
    if (!_available(candidate, sessionsAvailable))
    {
        return { LayoutHistoryStatus::SessionUnavailable, "A session required by this layout change is no longer available." };
    }
    if (!apply || !apply(candidate, false))
    {
        return { LayoutHistoryStatus::ApplyFailed, "The layout change could not be redone safely." };
    }
    auto entry = std::move(_redo.back());
    _redo.pop_back();
    _undo.emplace_back(_clone(entry));
    _trim();
    return { LayoutHistoryStatus::Applied, "The layout change was redone.", std::move(entry) };
}

void LayoutHistory::Clear() noexcept
{
    _undo.clear();
    _redo.clear();
}

LayoutHistoryEntry LayoutHistory::_clone(const LayoutHistoryEntry& entry)
{
    auto clone = entry;
    clone.before.sourceLayout = LayoutTree::Clone(entry.before.sourceLayout);
    clone.before.targetLayout = LayoutTree::Clone(entry.before.targetLayout);
    clone.after.sourceLayout = LayoutTree::Clone(entry.after.sourceLayout);
    clone.after.targetLayout = LayoutTree::Clone(entry.after.targetLayout);
    return clone;
}

bool LayoutHistory::_available(
    const LayoutHistoryEntry& entry,
    const SessionsAvailableCallback& callback)
{
    return callback && callback(entry.sessionIds);
}

void LayoutHistory::_trim()
{
    while (_undo.size() > _limit)
    {
        _undo.pop_front();
    }
    while (_redo.size() > _limit)
    {
        _redo.pop_front();
    }
}
