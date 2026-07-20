// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "MovePaneAction.h"

#include "../Docking/Layout/LayoutTree.h"

using namespace winTerm::Actions;
using namespace winTerm::Docking;
using namespace winTerm::Workspaces;

bool MovePanePlan::Succeeded() const noexcept
{
    return valid && invalidReason.empty() && targetRoot != nullptr;
}

MovePanePlan MovePaneAction::ToNewTab(const MovePaneContext& context)
{
    return _build(context, MovePaneDestination::NewTab);
}

MovePanePlan MovePaneAction::ToNewWindow(const MovePaneContext& context)
{
    return _build(context, MovePaneDestination::NewWindow);
}

MovePanePlan MovePaneAction::_build(
    const MovePaneContext& context,
    const MovePaneDestination destination)
{
    MovePanePlan result;
    result.destination = destination;
    if (!context.sourceLayout || context.sourcePaneId.empty())
    {
        result.invalidReason = "The source pane is unavailable.";
        return result;
    }
    const LayoutTree tree{ context.sourceLayout };
    const auto sourcePane = tree.Find(context.sourcePaneId);
    if (!sourcePane || sourcePane->type != LayoutNodeType::Pane)
    {
        result.invalidReason = "The source pane no longer exists.";
        return result;
    }
    if (destination == MovePaneDestination::NewTab && tree.PaneCount() <= 1)
    {
        result.invalidReason = "This pane is already the only pane in its tab.";
        return result;
    }

    DockSource source;
    source.type = DockSourceType::Pane;
    source.windowId = context.sourceWindowId;
    source.tabId = context.sourceTabId;
    source.paneId = context.sourcePaneId;
    source.root = LayoutTree::Clone(sourcePane);
    source.paneIds = { context.sourcePaneId };
    source.activePaneId = context.sourcePaneId;

    DockTarget target;
    target.type = destination == MovePaneDestination::NewWindow ?
                      DockTargetType::NewWindow :
                      DockTargetType::TabStrip;
    target.windowId = destination == MovePaneDestination::NewWindow ?
                          context.targetWindowId :
                          context.sourceWindowId;
    target.tabId = context.targetTabId;

    auto capabilities = context.capabilities;
    capabilities.sourcePaneCount = 1;
    capabilities.currentPaneCount = 0;
    if (!capabilities.Supports(source, target, DockZone::Center, context.sameProcess))
    {
        result.invalidReason = capabilities.DisabledReason(source, target, DockZone::Center, context.sameProcess);
        return result;
    }

    const auto removed = LayoutTransformer::Remove(context.sourceLayout, context.sourcePaneId);
    if (!removed.Succeeded())
    {
        result.invalidReason = removed.error;
        return result;
    }

    result.docking.source = std::move(source);
    result.docking.target = std::move(target);
    result.docking.zone = DockZone::Center;
    result.docking.operation = destination == MovePaneDestination::NewWindow ?
                                   DockOperation::MoveToNewWindow :
                                   DockOperation::MoveToNewTab;
    result.docking.status = DockingStatus::Ready;
    result.docking.proposedSourceLayout = LayoutTree::Clone(removed.root);
    result.docking.proposedTargetLayout = LayoutTree::Clone(sourcePane);
    result.docking.movedPaneIds = { context.sourcePaneId };
    result.sourceAfter = LayoutTree::Clone(removed.root);
    result.targetRoot = LayoutTree::Clone(sourcePane);
    result.sessionPaneIds = { context.sourcePaneId };
    result.historyDescription = destination == MovePaneDestination::NewWindow ?
                                    "Move pane to new window" :
                                    "Move pane to new tab";
    result.valid = true;
    return result;
}
