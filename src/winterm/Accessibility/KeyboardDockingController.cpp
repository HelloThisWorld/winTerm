// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "KeyboardDockingController.h"

#include <algorithm>
#include <array>
#include <utility>

using namespace winTerm::Accessibility;
using namespace winTerm::Docking;

namespace
{
    struct ZoneCoordinate
    {
        DockZone zone;
        int column;
        int row;
    };

    constexpr std::array ZoneCoordinates{
        ZoneCoordinate{ DockZone::TopLeft, 0, 0 },
        ZoneCoordinate{ DockZone::Top, 1, 0 },
        ZoneCoordinate{ DockZone::TopRight, 2, 0 },
        ZoneCoordinate{ DockZone::Left, 0, 1 },
        ZoneCoordinate{ DockZone::Center, 1, 1 },
        ZoneCoordinate{ DockZone::Right, 2, 1 },
        ZoneCoordinate{ DockZone::BottomLeft, 0, 2 },
        ZoneCoordinate{ DockZone::Bottom, 1, 2 },
        ZoneCoordinate{ DockZone::BottomRight, 2, 2 },
    };

    const ZoneCoordinate* CoordinateFor(const DockZone zone) noexcept
    {
        const auto result = std::find_if(
            ZoneCoordinates.begin(),
            ZoneCoordinates.end(),
            [zone](const auto& item) { return item.zone == zone; });
        return result == ZoneCoordinates.end() ? nullptr : &*result;
    }
}

bool KeyboardDockingController::Start(
    std::string sourceName,
    std::string targetName,
    std::vector<DockZone> availableZones,
    const DockZone initialZone)
{
    Reset();
    if (sourceName.empty() || targetName.empty() || availableZones.empty())
    {
        return false;
    }
    std::sort(
        availableZones.begin(),
        availableZones.end(),
        [](const auto first, const auto second) {
            return static_cast<int>(first) < static_cast<int>(second);
        });
    availableZones.erase(std::unique(availableZones.begin(), availableZones.end()), availableZones.end());

    _sourceName = std::move(sourceName);
    _targetName = std::move(targetName);
    _availableZones = std::move(availableZones);
    _state = KeyboardDockingState::SelectingZone;
    if (!_select(initialZone))
    {
        _selectedZone = _availableZones.front();
    }
    return true;
}

bool KeyboardDockingController::Handle(const DockingNavigationKey key)
{
    if (_state != KeyboardDockingState::SelectingZone)
    {
        return false;
    }
    switch (key)
    {
    case DockingNavigationKey::Left: return _move(-1, 0);
    case DockingNavigationKey::Right: return _move(1, 0);
    case DockingNavigationKey::Up: return _move(0, -1);
    case DockingNavigationKey::Down: return _move(0, 1);
    case DockingNavigationKey::Enter:
        _state = KeyboardDockingState::CommitRequested;
        return true;
    case DockingNavigationKey::Escape:
        _state = KeyboardDockingState::Cancelled;
        return true;
    default:
        return false;
    }
}

void KeyboardDockingController::Reset() noexcept
{
    _state = KeyboardDockingState::Inactive;
    _sourceName.clear();
    _targetName.clear();
    _availableZones.clear();
    _selectedZone.reset();
}

KeyboardDockingState KeyboardDockingController::State() const noexcept
{
    return _state;
}

std::optional<DockZone> KeyboardDockingController::SelectedZone() const noexcept
{
    return _selectedZone;
}

std::string KeyboardDockingController::Announcement() const
{
    if (_state == KeyboardDockingState::Inactive)
    {
        return {};
    }
    if (_state == KeyboardDockingState::Cancelled)
    {
        return "Docking cancelled. The original layout is unchanged.";
    }
    if (!_selectedZone)
    {
        return "No docking target is available.";
    }

    auto result = "Moving " + _sourceName + ". Target: " +
                  std::string{ AccessibleZoneName(*_selectedZone) } + " of " + _targetName + ".";
    if (_state == KeyboardDockingState::CommitRequested)
    {
        result += " Docking requested.";
    }
    else
    {
        result += " Press Enter to dock. Press Escape to cancel.";
    }
    return result;
}

std::string_view KeyboardDockingController::AccessibleZoneName(const DockZone zone) noexcept
{
    switch (zone)
    {
    case DockZone::Center: return "new tab area";
    case DockZone::Left: return "left side";
    case DockZone::Right: return "right side";
    case DockZone::Top: return "top side";
    case DockZone::Bottom: return "bottom side";
    case DockZone::TopLeft: return "top-left corner";
    case DockZone::TopRight: return "top-right corner";
    case DockZone::BottomLeft: return "bottom-left corner";
    case DockZone::BottomRight: return "bottom-right corner";
    default: return "unknown target";
    }
}

const std::vector<DockingCommandDescriptor>& KeyboardDockingController::Commands()
{
    static const std::vector<DockingCommandDescriptor> commands{
        { "winTerm.moveTabToNewWindow", "Move tab to new window", std::nullopt, DockSourceType::Tab },
        { "winTerm.moveTabToPreviousWindow", "Move tab to previous window", std::nullopt, DockSourceType::Tab },
        { "winTerm.moveTabToNextWindow", "Move tab to next window", std::nullopt, DockSourceType::Tab },
        { "winTerm.dockTabLeft", "Dock tab left", DockZone::Left, DockSourceType::Tab },
        { "winTerm.dockTabRight", "Dock tab right", DockZone::Right, DockSourceType::Tab },
        { "winTerm.dockTabTop", "Dock tab top", DockZone::Top, DockSourceType::Tab },
        { "winTerm.dockTabBottom", "Dock tab bottom", DockZone::Bottom, DockSourceType::Tab },
        { "winTerm.dockTabTopLeft", "Dock tab top left", DockZone::TopLeft, DockSourceType::Tab },
        { "winTerm.dockTabTopRight", "Dock tab top right", DockZone::TopRight, DockSourceType::Tab },
        { "winTerm.dockTabBottomLeft", "Dock tab bottom left", DockZone::BottomLeft, DockSourceType::Tab },
        { "winTerm.dockTabBottomRight", "Dock tab bottom right", DockZone::BottomRight, DockSourceType::Tab },
        { "winTerm.movePaneToNewTab", "Move pane to new tab", std::nullopt, DockSourceType::Pane },
        { "winTerm.movePaneToNewWindow", "Move pane to new window", std::nullopt, DockSourceType::Pane },
        { "winTerm.dockPaneLeft", "Dock pane left", DockZone::Left, DockSourceType::Pane },
        { "winTerm.dockPaneRight", "Dock pane right", DockZone::Right, DockSourceType::Pane },
        { "winTerm.dockPaneTop", "Dock pane top", DockZone::Top, DockSourceType::Pane },
        { "winTerm.dockPaneBottom", "Dock pane bottom", DockZone::Bottom, DockSourceType::Pane },
        { "winTerm.undoLayoutChange", "Undo layout change", std::nullopt, DockSourceType::LayoutSubtree },
        { "winTerm.redoLayoutChange", "Redo layout change", std::nullopt, DockSourceType::LayoutSubtree },
    };
    return commands;
}

bool KeyboardDockingController::_move(const int columnDelta, const int rowDelta)
{
    if (!_selectedZone)
    {
        return false;
    }
    const auto* current = CoordinateFor(*_selectedZone);
    if (!current)
    {
        return false;
    }

    const ZoneCoordinate* nearest = nullptr;
    auto nearestDistance = 100;
    for (const auto& candidate : ZoneCoordinates)
    {
        if (std::find(_availableZones.begin(), _availableZones.end(), candidate.zone) == _availableZones.end())
        {
            continue;
        }
        const auto columnDistance = candidate.column - current->column;
        const auto rowDistance = candidate.row - current->row;
        if ((columnDelta < 0 && columnDistance >= 0) ||
            (columnDelta > 0 && columnDistance <= 0) ||
            (rowDelta < 0 && rowDistance >= 0) ||
            (rowDelta > 0 && rowDistance <= 0))
        {
            continue;
        }
        if (columnDelta != 0 && rowDistance != 0)
        {
            continue;
        }
        if (rowDelta != 0 && columnDistance != 0)
        {
            continue;
        }
        const auto distance = std::abs(columnDistance) + std::abs(rowDistance);
        if (distance < nearestDistance)
        {
            nearest = &candidate;
            nearestDistance = distance;
        }
    }
    return nearest ? _select(nearest->zone) : false;
}

bool KeyboardDockingController::_select(const DockZone zone)
{
    if (std::find(_availableZones.begin(), _availableZones.end(), zone) == _availableZones.end())
    {
        return false;
    }
    _selectedZone = zone;
    return true;
}
