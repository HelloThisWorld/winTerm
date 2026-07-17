// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "LayoutTree.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

using namespace winTerm::Docking;
using namespace winTerm::Workspaces;

namespace
{
    LayoutNodePtr CloneNode(
        const LayoutNodePtr& node,
        std::unordered_map<const LayoutNode*, LayoutNodePtr>& clones)
    {
        if (!node)
        {
            return nullptr;
        }
        if (const auto found = clones.find(node.get()); found != clones.end())
        {
            return found->second;
        }
        auto clone = std::make_shared<LayoutNode>(*node);
        clone->first.reset();
        clone->second.reset();
        clones.emplace(node.get(), clone);
        clone->first = CloneNode(node->first, clones);
        clone->second = CloneNode(node->second, clones);
        return clone;
    }

    template<typename Callback>
    void Walk(
        const LayoutNodePtr& node,
        Callback&& callback,
        std::unordered_set<const LayoutNode*>& visited,
        const size_t depth)
    {
        if (!node || !visited.emplace(node.get()).second)
        {
            return;
        }
        callback(node, depth);
        Walk(node->first, callback, visited, depth + 1);
        Walk(node->second, callback, visited, depth + 1);
    }
}

LayoutTree::LayoutTree(LayoutNodePtr root) :
    _root{ std::move(root) }
{
}

const LayoutNodePtr& LayoutTree::Root() const noexcept
{
    return _root;
}

void LayoutTree::Root(LayoutNodePtr root)
{
    _root = std::move(root);
}

size_t LayoutTree::NodeCount() const noexcept
{
    size_t count{};
    std::unordered_set<const LayoutNode*> visited;
    Walk(_root, [&](const auto&, const auto) { ++count; }, visited, 1);
    return count;
}

size_t LayoutTree::PaneCount() const noexcept
{
    size_t count{};
    std::unordered_set<const LayoutNode*> visited;
    Walk(_root, [&](const auto& node, const auto) {
        if (node->type == LayoutNodeType::Pane)
        {
            ++count;
        }
    }, visited, 1);
    return count;
}

size_t LayoutTree::EmptySlotCount() const noexcept
{
    size_t count{};
    std::unordered_set<const LayoutNode*> visited;
    Walk(_root, [&](const auto& node, const auto) {
        if (node->type == LayoutNodeType::EmptySlot)
        {
            ++count;
        }
    }, visited, 1);
    return count;
}

size_t LayoutTree::MaximumDepth() const noexcept
{
    size_t maximum{};
    std::unordered_set<const LayoutNode*> visited;
    Walk(_root, [&](const auto&, const size_t depth) { maximum = std::max(maximum, depth); }, visited, 1);
    return maximum;
}

std::vector<std::string> LayoutTree::PaneIds() const
{
    std::vector<std::string> result;
    std::unordered_set<const LayoutNode*> visited;
    Walk(_root, [&](const auto& node, const auto) {
        if (node->type == LayoutNodeType::Pane)
        {
            result.emplace_back(node->paneId);
        }
    }, visited, 1);
    return result;
}

std::vector<std::string> LayoutTree::EmptySlotIds() const
{
    std::vector<std::string> result;
    std::unordered_set<const LayoutNode*> visited;
    Walk(_root, [&](const auto& node, const auto) {
        if (node->type == LayoutNodeType::EmptySlot)
        {
            result.emplace_back(node->slotId);
        }
    }, visited, 1);
    return result;
}

LayoutNodePtr LayoutTree::Find(const std::string_view nodeId) const noexcept
{
    LayoutNodePtr result;
    std::unordered_set<const LayoutNode*> visited;
    Walk(_root, [&](const auto& node, const auto) {
        if (!result)
        {
            const auto id = NodeId(node);
            if (id && *id == nodeId)
            {
                result = node;
            }
        }
    }, visited, 1);
    return result;
}

std::optional<std::string_view> LayoutTree::NodeId(const LayoutNodePtr& node) noexcept
{
    if (!node)
    {
        return std::nullopt;
    }
    if (node->type == LayoutNodeType::Pane)
    {
        return node->paneId;
    }
    if (node->type == LayoutNodeType::EmptySlot)
    {
        return node->slotId;
    }
    return std::nullopt;
}

LayoutNodePtr LayoutTree::Clone(const LayoutNodePtr& root)
{
    std::unordered_map<const LayoutNode*, LayoutNodePtr> clones;
    return CloneNode(root, clones);
}
