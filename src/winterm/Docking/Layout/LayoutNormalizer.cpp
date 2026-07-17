// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "LayoutNormalizer.h"

#include <unordered_set>

using namespace winTerm::Docking;
using namespace winTerm::Workspaces;

namespace
{
    LayoutNodePtr NormalizeNode(
        const LayoutNodePtr& node,
        LayoutNormalizationResult& result,
        std::unordered_set<const LayoutNode*>& ancestors)
    {
        if (!node)
        {
            return nullptr;
        }
        if (!ancestors.emplace(node.get()).second)
        {
            return node;
        }

        if (node->type != LayoutNodeType::Split)
        {
            ancestors.erase(node.get());
            return node;
        }

        node->first = NormalizeNode(node->first, result, ancestors);
        node->second = NormalizeNode(node->second, result, ancestors);
        ancestors.erase(node.get());

        if (!node->first && !node->second)
        {
            result.changed = true;
            ++result.collapsedNodes;
            return nullptr;
        }
        if (!node->first)
        {
            result.changed = true;
            ++result.collapsedNodes;
            return node->second;
        }
        if (!node->second)
        {
            result.changed = true;
            ++result.collapsedNodes;
            return node->first;
        }
        if (node->first->type == LayoutNodeType::EmptySlot &&
            node->second->type == LayoutNodeType::EmptySlot)
        {
            result.changed = true;
            ++result.collapsedNodes;
            return node->first;
        }
        return node;
    }
}

LayoutNormalizationResult LayoutNormalizer::Normalize(
    const LayoutNodePtr& root,
    std::string fallbackSlotId)
{
    LayoutNormalizationResult result;
    result.root = LayoutTree::Clone(root);
    std::unordered_set<const LayoutNode*> ancestors;
    result.root = NormalizeNode(result.root, result, ancestors);
    if (!result.root && !fallbackSlotId.empty())
    {
        result.root = LayoutNodeDescriptor::EmptySlot(std::move(fallbackSlotId));
        result.changed = true;
    }
    return result;
}
