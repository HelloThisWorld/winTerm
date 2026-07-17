// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "DockTargetResolver.h"

#include <algorithm>
#include <cmath>

using namespace winTerm::Docking;

DockTargetResolver::DockTargetResolver(DockTargetHysteresisSettings settings) :
    _settings{ std::move(settings) }
{
}

std::optional<DockTargetCandidate> DockTargetResolver::Resolve(
    const std::vector<DockTargetCandidate>& candidates,
    const DragPoint pointer,
    const std::chrono::steady_clock::time_point now)
{
    std::optional<DockTargetCandidate> best;
    for (const auto& candidate : candidates)
    {
        if (!candidate.enabled || candidate.key.empty() || !_contains(candidate.bounds, pointer))
        {
            continue;
        }
        if (!best ||
            Priority(candidate.target.type) < Priority(best->target.type) ||
            (Priority(candidate.target.type) == Priority(best->target.type) &&
             candidate.bounds.width * candidate.bounds.height < best->bounds.width * best->bounds.height))
        {
            best = candidate;
        }
    }

    if (!best)
    {
        Clear();
        return std::nullopt;
    }
    if (_current && _current->key == best->key && _current->zone == best->zone)
    {
        _pending.reset();
        return _current;
    }
    if (!_current)
    {
        _current = best;
        return _current;
    }

    if (!_pending || _pending->key != best->key || _pending->zone != best->zone)
    {
        _pending = best;
        _pendingSince = now;
        _pendingStart = pointer;
        return _current;
    }
    if (now - _pendingSince >= _settings.dwellTime ||
        _distance(_pendingStart, pointer) >= _settings.minimumDistance)
    {
        _current = _pending;
        _pending.reset();
    }
    return _current;
}

std::optional<DockTargetCandidate> DockTargetResolver::Current() const
{
    return _current;
}

void DockTargetResolver::Clear() noexcept
{
    _current.reset();
    _pending.reset();
}

int DockTargetResolver::Priority(const DockTargetType type) noexcept
{
    switch (type)
    {
    case DockTargetType::TabStrip: return 0;
    case DockTargetType::EmptySlot: return 1;
    case DockTargetType::Pane: return 2;
    case DockTargetType::WindowContent: return 3;
    case DockTargetType::NewWindow: return 4;
    default: return 5;
    }
}

bool DockTargetResolver::_contains(const LayoutRect& bounds, const DragPoint pointer) noexcept
{
    return std::isfinite(pointer.x) &&
           std::isfinite(pointer.y) &&
           pointer.x >= bounds.x &&
           pointer.y >= bounds.y &&
           pointer.x <= bounds.x + bounds.width &&
           pointer.y <= bounds.y + bounds.height;
}

double DockTargetResolver::_distance(const DragPoint first, const DragPoint second) noexcept
{
    return std::hypot(second.x - first.x, second.y - first.y);
}
