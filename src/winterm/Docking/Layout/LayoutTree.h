// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../../Workspaces/Model/WorkspaceDescriptor.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace winTerm::Docking
{
    using LayoutNode = Workspaces::LayoutNodeDescriptor;
    using LayoutNodePtr = std::shared_ptr<LayoutNode>;

    class LayoutTree
    {
    public:
        LayoutTree() = default;
        explicit LayoutTree(LayoutNodePtr root);

        const LayoutNodePtr& Root() const noexcept;
        void Root(LayoutNodePtr root);

        size_t NodeCount() const noexcept;
        size_t PaneCount() const noexcept;
        size_t EmptySlotCount() const noexcept;
        size_t MaximumDepth() const noexcept;

        std::vector<std::string> PaneIds() const;
        std::vector<std::string> EmptySlotIds() const;
        LayoutNodePtr Find(std::string_view nodeId) const noexcept;

        static std::optional<std::string_view> NodeId(const LayoutNodePtr& node) noexcept;
        static LayoutNodePtr Clone(const LayoutNodePtr& root);

    private:
        LayoutNodePtr _root;
    };
}
