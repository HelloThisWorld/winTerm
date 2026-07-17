// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "DockingModel.h"

#include <array>

using namespace winTerm::Docking;

namespace
{
    bool IsCorner(const DockZone zone) noexcept
    {
        return zone == DockZone::TopLeft ||
               zone == DockZone::TopRight ||
               zone == DockZone::BottomLeft ||
               zone == DockZone::BottomRight;
    }

    bool IsEdge(const DockZone zone) noexcept
    {
        return zone == DockZone::Left ||
               zone == DockZone::Right ||
               zone == DockZone::Top ||
               zone == DockZone::Bottom;
    }
}

bool DockingCapabilities::Supports(
    const DockSource& source,
    const DockTarget& target,
    const DockZone zone,
    const bool sameProcess) const noexcept
{
    if (!canMove || targetClosing || readOnlyRestricted)
    {
        return false;
    }
    if (!sameProcess || !canTransferAcrossProcess)
    {
        if (!sameProcess)
        {
            return false;
        }
    }
    if (source.type == DockSourceType::Tab && !canTransferLiveTab)
    {
        return false;
    }
    if (source.type == DockSourceType::Pane && !canTransferLivePane)
    {
        return false;
    }
    if (IsEdge(zone) && !canDockEdges)
    {
        return false;
    }
    if (IsCorner(zone) && (!canDockCorners || !canUseEmptySlots))
    {
        return false;
    }
    if (target.type == DockTargetType::EmptySlot && !canUseEmptySlots)
    {
        return false;
    }
    return currentPaneCount + sourcePaneCount <= maximumPaneCount;
}

std::string DockingCapabilities::DisabledReason(
    const DockSource& source,
    const DockTarget& target,
    const DockZone zone,
    const bool sameProcess) const
{
    if (!sameProcess)
    {
        return "Live terminal transfer is available only between windows in this winTerm process.";
    }
    if (targetClosing)
    {
        return "The target is closing.";
    }
    if (readOnlyRestricted)
    {
        return "The target does not allow layout changes.";
    }
    if (!canMove)
    {
        return "This content cannot be moved.";
    }
    if (source.type == DockSourceType::Tab && !canTransferLiveTab)
    {
        return "This tab cannot be transferred while its sessions are running.";
    }
    if (source.type == DockSourceType::Pane && !canTransferLivePane)
    {
        return "This pane cannot be transferred while its session is running.";
    }
    if (IsEdge(zone) && !canDockEdges)
    {
        return "Edge docking is disabled.";
    }
    if (IsCorner(zone) && !canDockCorners)
    {
        return "Corner docking is disabled.";
    }
    if ((IsCorner(zone) || target.type == DockTargetType::EmptySlot) && !canUseEmptySlots)
    {
        return "Empty layout slots are disabled.";
    }
    if (currentPaneCount + sourcePaneCount > maximumPaneCount)
    {
        return "The drop would exceed the supported pane limit.";
    }
    return Supports(source, target, zone, sameProcess) ? std::string{} : "The selected drop target is not available.";
}

bool DockingResult::Succeeded() const noexcept
{
    return status == DockingStatus::Committed;
}

std::string_view winTerm::Docking::ToString(const DockSourceType value) noexcept
{
    switch (value)
    {
    case DockSourceType::Tab: return "tab";
    case DockSourceType::Pane: return "pane";
    case DockSourceType::LayoutSubtree: return "layoutSubtree";
    default: return "unknown";
    }
}

std::string_view winTerm::Docking::ToString(const DockTargetType value) noexcept
{
    switch (value)
    {
    case DockTargetType::TabStrip: return "tabStrip";
    case DockTargetType::WindowContent: return "windowContent";
    case DockTargetType::Pane: return "pane";
    case DockTargetType::EmptySlot: return "emptySlot";
    case DockTargetType::NewWindow: return "newWindow";
    default: return "unknown";
    }
}

std::string_view winTerm::Docking::ToString(const DockZone value) noexcept
{
    switch (value)
    {
    case DockZone::Center: return "center";
    case DockZone::Left: return "left";
    case DockZone::Right: return "right";
    case DockZone::Top: return "top";
    case DockZone::Bottom: return "bottom";
    case DockZone::TopLeft: return "topLeft";
    case DockZone::TopRight: return "topRight";
    case DockZone::BottomLeft: return "bottomLeft";
    case DockZone::BottomRight: return "bottomRight";
    default: return "unknown";
    }
}

std::optional<DockZone> winTerm::Docking::DockZoneFromString(const std::string_view value) noexcept
{
    static constexpr std::array values{
        std::pair{ DockZone::Center, std::string_view{ "center" } },
        std::pair{ DockZone::Left, std::string_view{ "left" } },
        std::pair{ DockZone::Right, std::string_view{ "right" } },
        std::pair{ DockZone::Top, std::string_view{ "top" } },
        std::pair{ DockZone::Bottom, std::string_view{ "bottom" } },
        std::pair{ DockZone::TopLeft, std::string_view{ "topLeft" } },
        std::pair{ DockZone::TopRight, std::string_view{ "topRight" } },
        std::pair{ DockZone::BottomLeft, std::string_view{ "bottomLeft" } },
        std::pair{ DockZone::BottomRight, std::string_view{ "bottomRight" } },
    };
    for (const auto& [zone, name] : values)
    {
        if (name == value)
        {
            return zone;
        }
    }
    return std::nullopt;
}
