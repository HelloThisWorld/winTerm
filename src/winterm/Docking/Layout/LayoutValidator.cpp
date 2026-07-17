// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "LayoutValidator.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

using namespace winTerm::Docking;
using namespace winTerm::Workspaces;

namespace
{
    void AddError(
        LayoutValidationResult& result,
        std::string code,
        std::string nodeId,
        std::string message)
    {
        result.issues.emplace_back(LayoutValidationIssue{
            LayoutIssueSeverity::Error,
            std::move(code),
            std::move(nodeId),
            std::move(message),
        });
    }

    void ValidateNode(
        const LayoutNodePtr& node,
        const LayoutValidationOptions& options,
        LayoutValidationResult& result,
        std::unordered_set<const LayoutNode*>& ancestors,
        std::unordered_set<std::string>& paneIds,
        std::unordered_set<std::string>& slotIds,
        std::unordered_set<std::string>& stableIds,
        const size_t depth)
    {
        if (!node)
        {
            AddError(result, "missingNode", {}, "The layout contains a missing node.");
            return;
        }
        ++result.nodeCount;
        result.maximumDepth = std::max(result.maximumDepth, depth);
        if (depth > options.maximumDepth)
        {
            AddError(result, "layoutTooDeep", {}, "The layout exceeds the supported nesting depth.");
            return;
        }
        if (!ancestors.emplace(node.get()).second)
        {
            AddError(result, "layoutCycle", {}, "The layout contains a cycle.");
            return;
        }

        switch (node->type)
        {
        case LayoutNodeType::Pane:
            ++result.paneCount;
            if (node->paneId.empty())
            {
                AddError(result, "missingPaneId", {}, "A pane node is missing its stable pane ID.");
            }
            else if (!paneIds.emplace(node->paneId).second)
            {
                AddError(result, "duplicatePaneId", node->paneId, "A pane ID appears more than once in the layout.");
            }
            else if (!stableIds.emplace(node->paneId).second)
            {
                AddError(result, "duplicateNodeId", node->paneId, "A stable node ID is used by more than one layout node.");
            }
            if (node->first || node->second || !node->slotId.empty() || node->preferredProfileId)
            {
                AddError(result, "invalidPaneNode", node->paneId, "A pane node contains split or empty-slot data.");
            }
            if (options.requireLiveSessionForEveryPane && !options.liveSessionPaneIds.contains(node->paneId))
            {
                AddError(result, "orphanPane", node->paneId, "The pane does not reference a live terminal session.");
            }
            break;
        case LayoutNodeType::EmptySlot:
            ++result.emptySlotCount;
            if (node->slotId.empty())
            {
                AddError(result, "missingSlotId", {}, "An empty slot is missing its stable slot ID.");
            }
            else if (!slotIds.emplace(node->slotId).second)
            {
                AddError(result, "duplicateSlotId", node->slotId, "An empty slot ID appears more than once in the layout.");
            }
            else if (!stableIds.emplace(node->slotId).second)
            {
                AddError(result, "duplicateNodeId", node->slotId, "A stable node ID is used by more than one layout node.");
            }
            if (node->first || node->second || !node->paneId.empty())
            {
                AddError(result, "invalidEmptySlot", node->slotId, "An empty slot contains pane or split data.");
            }
            break;
        case LayoutNodeType::Split:
            if (!std::isfinite(node->ratio) ||
                node->ratio < MinimumSplitRatio ||
                node->ratio > MaximumSplitRatio)
            {
                AddError(result, "invalidSplitRatio", {}, "A split ratio is outside the supported range.");
            }
            if (!node->paneId.empty() || !node->slotId.empty() || node->preferredProfileId)
            {
                AddError(result, "invalidSplitNode", {}, "A split contains pane or empty-slot metadata.");
            }
            ValidateNode(node->first, options, result, ancestors, paneIds, slotIds, stableIds, depth + 1);
            ValidateNode(node->second, options, result, ancestors, paneIds, slotIds, stableIds, depth + 1);
            break;
        default:
            AddError(result, "unsupportedNode", {}, "The layout contains an unsupported node type.");
            break;
        }

        ancestors.erase(node.get());
    }
}

bool LayoutValidationResult::IsValid() const noexcept
{
    return std::none_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == LayoutIssueSeverity::Error;
    });
}

std::vector<std::string> LayoutValidationResult::ErrorMessages() const
{
    std::vector<std::string> result;
    for (const auto& issue : issues)
    {
        if (issue.severity == LayoutIssueSeverity::Error)
        {
            result.emplace_back(issue.message);
        }
    }
    return result;
}

LayoutValidationResult LayoutValidator::Validate(
    const LayoutNodePtr& root,
    const LayoutValidationOptions& options)
{
    LayoutValidationResult result;
    std::unordered_set<const LayoutNode*> ancestors;
    std::unordered_set<std::string> paneIds;
    std::unordered_set<std::string> slotIds;
    std::unordered_set<std::string> stableIds;
    ValidateNode(root, options, result, ancestors, paneIds, slotIds, stableIds, 1);
    if (result.paneCount > options.maximumPanes)
    {
        AddError(result, "tooManyPanes", {}, "The layout exceeds the supported pane limit.");
    }
    return result;
}
