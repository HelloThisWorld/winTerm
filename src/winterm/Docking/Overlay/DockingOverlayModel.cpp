// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "DockingOverlayModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

using namespace winTerm::Docking;
using namespace winTerm::Workspaces;

namespace
{
    struct ZoneDefinition
    {
        DockZone zone;
        int column;
        int row;
        std::string_view label;
        std::string_view automationName;
        std::string_view icon;
    };

    constexpr std::array ZoneDefinitions{
        ZoneDefinition{ DockZone::TopLeft, 0, 0, "Top left", "Dock to the top-left corner", "↖" },
        ZoneDefinition{ DockZone::Top, 1, 0, "Top", "Dock above the active target", "↑" },
        ZoneDefinition{ DockZone::TopRight, 2, 0, "Top right", "Dock to the top-right corner", "↗" },
        ZoneDefinition{ DockZone::Left, 0, 1, "Left", "Dock to the left of the active target", "←" },
        ZoneDefinition{ DockZone::Center, 1, 1, "New tab", "Move as a new tab", "●" },
        ZoneDefinition{ DockZone::Right, 2, 1, "Right", "Dock to the right of the active target", "→" },
        ZoneDefinition{ DockZone::BottomLeft, 0, 2, "Bottom left", "Dock to the bottom-left corner", "↙" },
        ZoneDefinition{ DockZone::Bottom, 1, 2, "Bottom", "Dock below the active target", "↓" },
        ZoneDefinition{ DockZone::BottomRight, 2, 2, "Bottom right", "Dock to the bottom-right corner", "↘" },
    };

    bool IsCorner(const DockZone zone) noexcept
    {
        return zone == DockZone::TopLeft ||
               zone == DockZone::TopRight ||
               zone == DockZone::BottomLeft ||
               zone == DockZone::BottomRight;
    }
}

std::vector<DockZonePresentation> DockingOverlayModel::Build(
    const LayoutRect contentBounds,
    const DockSource& source,
    const DockTarget& target,
    const DockingCapabilities& capabilities,
    const bool sameProcess,
    const std::optional<DockZone> selected,
    const DockingOverlaySettings& settings)
{
    std::vector<DockZonePresentation> result;
    if (!std::isfinite(contentBounds.width) ||
        !std::isfinite(contentBounds.height) ||
        contentBounds.width <= 0 ||
        contentBounds.height <= 0)
    {
        return result;
    }
    const auto scale = std::max(0.25, settings.dpiScale);
    const auto tile = std::max(32.0, settings.tileSize * scale);
    const auto gap = std::max(0.0, settings.tileGap * scale);
    const auto overlayWidth = tile * 3.0 + gap * 2.0;
    const auto overlayHeight = tile * 3.0 + gap * 2.0;
    const auto originX = std::clamp(
        contentBounds.x + (contentBounds.width - overlayWidth) / 2.0,
        contentBounds.x,
        std::max(contentBounds.x, contentBounds.x + contentBounds.width - overlayWidth));
    const auto originY = std::clamp(
        contentBounds.y + (contentBounds.height - overlayHeight) / 2.0,
        contentBounds.y,
        std::max(contentBounds.y, contentBounds.y + contentBounds.height - overlayHeight));
    const auto cornersFit =
        contentBounds.width >= settings.minimumCornerTargetWidth * scale &&
        contentBounds.height >= settings.minimumCornerTargetHeight * scale;

    result.reserve(ZoneDefinitions.size());
    for (const auto& definition : ZoneDefinitions)
    {
        const auto visible = !IsCorner(definition.zone) || cornersFit;
        if (!visible)
        {
            continue;
        }
        const auto enabled = capabilities.Supports(source, target, definition.zone, sameProcess);
        DockZonePresentation presentation;
        presentation.zone = definition.zone;
        presentation.hitBounds = {
            originX + definition.column * (tile + gap),
            originY + definition.row * (tile + gap),
            tile,
            tile,
        };
        presentation.label = settings.showLabels ? std::string{ definition.label } : std::string{};
        presentation.automationName = definition.automationName;
        presentation.icon = definition.icon;
        presentation.enabled = enabled;
        presentation.selected = selected && *selected == definition.zone;
        presentation.visible = true;
        if (!enabled)
        {
            presentation.disabledReason = capabilities.DisabledReason(source, target, definition.zone, sameProcess);
            if (!presentation.disabledReason.empty())
            {
                presentation.automationName += ". " + presentation.disabledReason;
            }
        }
        result.emplace_back(std::move(presentation));
    }
    return result;
}

DockPreviewModel DockPreview::Build(
    const DockingPlan& plan,
    const LayoutRect targetBounds,
    const LayoutGeometrySettings& settings)
{
    DockPreviewModel result;
    if (plan.status != DockingStatus::Ready)
    {
        result.invalidReason = plan.invalidReason.empty() ? "The selected drop is not available." : plan.invalidReason;
        result.automationSummary = result.invalidReason;
        return result;
    }
    if (!plan.proposedTargetLayout)
    {
        result.invalidReason = "This operation does not have an in-window layout preview.";
        result.automationSummary = result.invalidReason;
        return result;
    }

    const auto geometry = LayoutGeometry::Calculate(plan.proposedTargetLayout, targetBounds, settings);
    if (!geometry.valid)
    {
        result.invalidReason = geometry.invalidReason;
        result.automationSummary = result.invalidReason;
        return result;
    }
    const std::unordered_set<std::string> moved{ plan.movedPaneIds.begin(), plan.movedPaneIds.end() };
    for (const auto& entry : geometry.entries)
    {
        if (entry.type == LayoutNodeType::Split)
        {
            continue;
        }
        DockPreviewRegion region;
        region.nodeId = entry.nodeId;
        region.bounds = entry.bounds;
        if (entry.type == LayoutNodeType::EmptySlot)
        {
            region.role = DockPreviewRegionRole::EmptySlot;
            region.automationName = "Empty layout slot";
        }
        else if (moved.contains(entry.nodeId))
        {
            region.role = DockPreviewRegionRole::Source;
            region.automationName = "Moved terminal content";
        }
        else
        {
            region.role = DockPreviewRegionRole::Target;
            region.automationName = "Existing terminal content";
        }
        result.regions.emplace_back(std::move(region));
    }
    result.valid = true;
    result.automationSummary = "Preview: " + std::string{ ToString(plan.zone) } + " docking";
    return result;
}
