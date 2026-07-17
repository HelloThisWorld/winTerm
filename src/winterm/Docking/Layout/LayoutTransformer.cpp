// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "LayoutTransformer.h"

#include "LayoutValidator.h"

#include <algorithm>
#include <functional>

using namespace winTerm::Docking;
using namespace winTerm::Workspaces;

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

    LayoutNodePtr BuildReplacement(
        const LayoutNodePtr& target,
        const LayoutNodePtr& source,
        const DockZone zone,
        std::string emptySlotId,
        const LayoutTransformSettings& settings,
        std::string& error)
    {
        if (!target || !source)
        {
            error = "The source or target layout is missing.";
            return nullptr;
        }
        if (target->type == LayoutNodeType::EmptySlot)
        {
            return LayoutTree::Clone(source);
        }
        if (zone == DockZone::Center)
        {
            error = "The center zone moves content as a tab and does not replace a pane.";
            return nullptr;
        }

        const auto sourceClone = LayoutTree::Clone(source);
        const auto targetClone = LayoutTree::Clone(target);
        if (IsEdge(zone))
        {
            const auto orientation =
                zone == DockZone::Left || zone == DockZone::Right ?
                    SplitOrientation::Vertical :
                    SplitOrientation::Horizontal;
            const auto sourceFirst = zone == DockZone::Left || zone == DockZone::Top;
            return LayoutNodeDescriptor::Split(
                orientation,
                settings.defaultSplitRatio,
                sourceFirst ? sourceClone : targetClone,
                sourceFirst ? targetClone : sourceClone);
        }
        if (!IsCorner(zone))
        {
            error = "The selected docking zone is not supported.";
            return nullptr;
        }
        if (emptySlotId.empty())
        {
            error = "Corner docking requires a stable empty-slot ID.";
            return nullptr;
        }

        const auto empty = LayoutNodeDescriptor::EmptySlot(std::move(emptySlotId));
        const auto sourceTop = zone == DockZone::TopLeft || zone == DockZone::TopRight;
        const auto sourceLeft = zone == DockZone::TopLeft || zone == DockZone::BottomLeft;
        const auto cornerColumn = LayoutNodeDescriptor::Split(
            SplitOrientation::Horizontal,
            settings.cornerHeightRatio,
            sourceTop ? sourceClone : empty,
            sourceTop ? empty : sourceClone);
        return LayoutNodeDescriptor::Split(
            SplitOrientation::Vertical,
            sourceLeft ? settings.cornerWidthRatio : 1.0 - settings.cornerWidthRatio,
            sourceLeft ? cornerColumn : targetClone,
            sourceLeft ? targetClone : cornerColumn);
    }

    LayoutNodePtr ReplaceById(
        const LayoutNodePtr& node,
        const std::string_view targetNodeId,
        const std::function<LayoutNodePtr(const LayoutNodePtr&)>& replacement,
        bool& replaced)
    {
        if (!node)
        {
            return nullptr;
        }
        const auto clone = std::make_shared<LayoutNode>(*node);
        clone->first.reset();
        clone->second.reset();
        const auto id = LayoutTree::NodeId(node);
        if (!replaced && id && *id == targetNodeId)
        {
            replaced = true;
            return replacement(node);
        }
        clone->first = ReplaceById(node->first, targetNodeId, replacement, replaced);
        clone->second = ReplaceById(node->second, targetNodeId, replacement, replaced);
        return clone;
    }

    LayoutNodePtr RemoveById(
        const LayoutNodePtr& node,
        const std::string_view sourceNodeId,
        LayoutNodePtr& removed,
        bool& changed)
    {
        if (!node)
        {
            return nullptr;
        }
        const auto id = LayoutTree::NodeId(node);
        if (!removed && id && *id == sourceNodeId)
        {
            removed = LayoutTree::Clone(node);
            changed = true;
            return nullptr;
        }
        const auto clone = std::make_shared<LayoutNode>(*node);
        clone->first = RemoveById(node->first, sourceNodeId, removed, changed);
        clone->second = RemoveById(node->second, sourceNodeId, removed, changed);
        return clone;
    }
}

bool LayoutTransformResult::Succeeded() const noexcept
{
    return error.empty() && changed;
}

LayoutTransformResult LayoutTransformer::Insert(
    const LayoutNodePtr& targetRoot,
    const std::optional<std::string_view> targetNodeId,
    const LayoutNodePtr& sourceRoot,
    const DockZone zone,
    std::string emptySlotId,
    const LayoutTransformSettings& settings)
{
    LayoutTransformResult result;
    if (!targetRoot || !sourceRoot)
    {
        result.error = "The source or target layout is missing.";
        return result;
    }

    if (!targetNodeId)
    {
        result.root = BuildReplacement(targetRoot, sourceRoot, zone, std::move(emptySlotId), settings, result.error);
        result.changed = result.root != nullptr;
    }
    else
    {
        bool replaced = false;
        result.root = ReplaceById(
            targetRoot,
            *targetNodeId,
            [&](const auto& target) {
                return BuildReplacement(target, sourceRoot, zone, std::move(emptySlotId), settings, result.error);
            },
            replaced);
        result.changed = replaced && result.error.empty();
        if (!replaced)
        {
            result.error = "The target layout node no longer exists.";
        }
    }

    if (result.changed)
    {
        const auto validation = LayoutValidator::Validate(result.root);
        if (!validation.IsValid())
        {
            result.error = validation.ErrorMessages().front();
            result.changed = false;
        }
    }
    return result;
}

LayoutTransformResult LayoutTransformer::MoveWithin(
    const LayoutNodePtr& root,
    const std::string_view sourceNodeId,
    const std::string_view targetNodeId,
    const DockZone zone,
    std::string emptySlotId,
    std::string fallbackSlotId,
    const LayoutTransformSettings& settings)
{
    LayoutTransformResult result;
    if (sourceNodeId == targetNodeId)
    {
        result.error = "The source and target layout nodes are the same.";
        return result;
    }
    auto removed = Remove(root, sourceNodeId, std::move(fallbackSlotId));
    if (!removed.Succeeded() || !removed.removed)
    {
        return removed;
    }
    auto inserted = Insert(
        removed.root,
        targetNodeId,
        removed.removed,
        zone,
        std::move(emptySlotId),
        settings);
    inserted.removed = removed.removed;
    return inserted;
}

LayoutTransformResult LayoutTransformer::Remove(
    const LayoutNodePtr& root,
    const std::string_view sourceNodeId,
    std::string fallbackSlotId)
{
    LayoutTransformResult result;
    bool changed = false;
    result.root = RemoveById(root, sourceNodeId, result.removed, changed);
    if (!changed)
    {
        result.error = "The source layout node no longer exists.";
        return result;
    }
    const auto normalized = LayoutNormalizer::Normalize(result.root, std::move(fallbackSlotId));
    result.root = normalized.root;
    result.changed = true;
    return result;
}

LayoutTransformResult LayoutTransformer::FillEmptySlot(
    const LayoutNodePtr& root,
    const std::string_view slotId,
    const LayoutNodePtr& sourceRoot)
{
    LayoutTransformResult result;
    bool replaced = false;
    result.root = ReplaceById(
        root,
        slotId,
        [&](const auto& target) {
            if (target->type != LayoutNodeType::EmptySlot)
            {
                result.error = "The selected target is not an empty layout slot.";
                return LayoutNodePtr{};
            }
            return LayoutTree::Clone(sourceRoot);
        },
        replaced);
    result.changed = replaced && result.error.empty() && result.root;
    if (!replaced)
    {
        result.error = "The empty layout slot no longer exists.";
    }
    return result;
}

DockingPlan LayoutTransformer::BuildProposedLayout(
    DockSource source,
    DockTarget target,
    const DockZone zone,
    const LayoutNodePtr& sourceLayout,
    const LayoutNodePtr& targetLayout,
    std::string emptySlotId,
    const DockingCapabilities& capabilities,
    const bool sameProcess,
    const LayoutTransformSettings& settings)
{
    DockingPlan plan;
    plan.source = std::move(source);
    plan.target = std::move(target);
    plan.zone = zone;
    plan.movedPaneIds = LayoutTree{ sourceLayout }.PaneIds();
    if (!capabilities.Supports(plan.source, plan.target, zone, sameProcess))
    {
        plan.invalidReason = capabilities.DisabledReason(plan.source, plan.target, zone, sameProcess);
        return plan;
    }
    if (plan.target.type == DockTargetType::TabStrip ||
        plan.target.type == DockTargetType::NewWindow ||
        zone == DockZone::Center)
    {
        plan.proposedSourceLayout = LayoutTree::Clone(sourceLayout);
        plan.proposedTargetLayout = LayoutTree::Clone(targetLayout);
        plan.status = DockingStatus::Ready;
        return plan;
    }
    const auto transformed = Insert(
        targetLayout,
        plan.target.nodeId ? std::optional<std::string_view>{ *plan.target.nodeId } : std::nullopt,
        sourceLayout,
        zone,
        std::move(emptySlotId),
        settings);
    if (!transformed.Succeeded())
    {
        plan.invalidReason = transformed.error;
        return plan;
    }
    plan.proposedTargetLayout = transformed.root;
    plan.status = DockingStatus::Ready;
    return plan;
}
