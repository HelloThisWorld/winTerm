// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "DirectedSplitAction.h"

#include "../Docking/Layout/LayoutTree.h"

#include <algorithm>
#include <cmath>

using namespace winTerm::Actions;
using namespace winTerm::Docking;
using namespace winTerm::Workspaces;

namespace
{
    std::string ResolveProfile(const DirectedSplitContext& context)
    {
        if (context.selectedProfileId && !context.selectedProfileId->empty())
        {
            return *context.selectedProfileId;
        }
        switch (context.profilePolicy)
        {
        case SplitProfilePolicy::DefaultProfile:
            return context.defaultProfileId;
        case SplitProfilePolicy::CurrentPaneProfile:
            return context.currentPaneProfileId;
        case SplitProfilePolicy::AskEveryTime:
        default:
            return {};
        }
    }

    std::string HistoryDescription(const DockZone direction)
    {
        return "Split pane " + std::string{ ToString(direction) };
    }
}

bool DirectedSplitPlan::Succeeded() const noexcept
{
    return valid && invalidReason.empty() && proposedLayout != nullptr;
}

DirectedSplitPlan DirectedSplitAction::BuildPlan(const DirectedSplitContext& context)
{
    DirectedSplitPlan result;
    result.focusNewPane = context.focusNewPane;
    if (!context.layout)
    {
        result.invalidReason = "The current tab layout is unavailable.";
        return result;
    }
    if (context.focusedPaneId.empty() || context.newPaneId.empty())
    {
        result.invalidReason = "The focused pane or new pane ID is missing.";
        return result;
    }
    const LayoutTree tree{ context.layout };
    const auto focused = tree.Find(context.focusedPaneId);
    if (!focused || focused->type != LayoutNodeType::Pane)
    {
        result.invalidReason = "The focused pane no longer exists.";
        return result;
    }
    if (tree.Find(context.newPaneId))
    {
        result.invalidReason = "The new pane ID is already in use.";
        return result;
    }
    if (!CanSplit(context.direction, context.focusedPaneBounds, context.geometrySettings, &result.invalidReason))
    {
        return result;
    }
    result.profileId = ResolveProfile(context);
    if (result.profileId.empty())
    {
        result.invalidReason = context.profilePolicy == SplitProfilePolicy::AskEveryTime ?
                                   "Select a profile before splitting the pane." :
                                   "The profile used for the split is unavailable.";
        return result;
    }

    DockSource source;
    source.type = DockSourceType::Pane;
    source.windowId = context.windowId;
    // A directed split plans a new pane that is not in the current tab yet.
    // Leaving source.tabId empty prevents same-tab move semantics.
    source.paneId = context.newPaneId;
    source.root = LayoutNodeDescriptor::Pane(context.newPaneId);
    source.paneIds = { context.newPaneId };
    source.activePaneId = context.newPaneId;

    DockTarget target;
    target.type = DockTargetType::Pane;
    target.windowId = context.windowId;
    target.tabId = context.tabId;
    target.nodeId = context.focusedPaneId;

    DockingCapabilities capabilities;
    capabilities.sourcePaneCount = 1;
    capabilities.currentPaneCount = tree.PaneCount();
    capabilities.canDockCorners = false;

    LayoutTransformSettings transformSettings;
    transformSettings.defaultSplitRatio = std::clamp(context.splitRatio, 0.2, 0.8);
    result.docking = LayoutTransformer::BuildProposedLayout(
        std::move(source),
        std::move(target),
        context.direction,
        LayoutNodeDescriptor::Pane(context.newPaneId),
        context.layout,
        {},
        capabilities,
        true,
        transformSettings);
    result.docking.operation = DockOperation::Split;
    if (result.docking.status != DockingStatus::Ready)
    {
        result.invalidReason = result.docking.invalidReason;
        return result;
    }

    const auto geometry = LayoutGeometry::Calculate(
        result.docking.proposedTargetLayout,
        context.tabBounds,
        context.geometrySettings);
    if (!geometry.valid)
    {
        result.invalidReason = geometry.invalidReason;
        result.docking.status = DockingStatus::Invalid;
        result.docking.invalidReason = result.invalidReason;
        return result;
    }

    result.proposedLayout = LayoutTree::Clone(result.docking.proposedTargetLayout);
    result.historyDescription = HistoryDescription(context.direction);
    result.valid = true;
    return result;
}

bool DirectedSplitAction::IsDirection(const DockZone direction) noexcept
{
    return direction == DockZone::Top ||
           direction == DockZone::Bottom ||
           direction == DockZone::Left ||
           direction == DockZone::Right;
}

bool DirectedSplitAction::CanSplit(
    const DockZone direction,
    const LayoutRect focusedPaneBounds,
    const LayoutGeometrySettings& geometrySettings,
    std::string* disabledReason) noexcept
{
    auto reject = [&](const std::string_view reason) {
        if (disabledReason)
        {
            *disabledReason = reason;
        }
        return false;
    };
    if (!IsDirection(direction))
    {
        return reject("Select Top, Bottom, Left, or Right.");
    }
    if (!std::isfinite(geometrySettings.dpiScale) || geometrySettings.dpiScale <= 0 ||
        !std::isfinite(focusedPaneBounds.width) || !std::isfinite(focusedPaneBounds.height))
    {
        return reject("The pane geometry or DPI is invalid.");
    }
    const auto scale = geometrySettings.dpiScale;
    if ((direction == DockZone::Left || direction == DockZone::Right) &&
        focusedPaneBounds.width < geometrySettings.minimumPaneWidth * scale * 2.0)
    {
        return reject("The focused pane is too narrow for this split.");
    }
    if ((direction == DockZone::Top || direction == DockZone::Bottom) &&
        focusedPaneBounds.height < geometrySettings.minimumPaneHeight * scale * 2.0)
    {
        return reject("The focused pane is too short for this split.");
    }
    if (disabledReason)
    {
        disabledReason->clear();
    }
    return true;
}

std::string_view DirectedSplitAction::CommandId(const DockZone direction) noexcept
{
    switch (direction)
    {
    case DockZone::Top: return "splitPaneTop";
    case DockZone::Bottom: return "splitPaneBottom";
    case DockZone::Left: return "splitPaneLeft";
    case DockZone::Right: return "splitPaneRight";
    default: return {};
    }
}

std::string_view DirectedSplitAction::AccessibleName(const DockZone direction) noexcept
{
    switch (direction)
    {
    case DockZone::Top: return "Split pane above";
    case DockZone::Bottom: return "Split pane below";
    case DockZone::Left: return "Split pane to the left";
    case DockZone::Right: return "Split pane to the right";
    default: return "Split pane";
    }
}
