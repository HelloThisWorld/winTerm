// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Model/DockingModel.h"
#include "LayoutNormalizer.h"

#include <optional>
#include <string>

namespace winTerm::Docking
{
    struct LayoutTransformResult
    {
        LayoutNodePtr root;
        LayoutNodePtr removed;
        bool changed{ false };
        std::string error;

        bool Succeeded() const noexcept;
    };

    struct LayoutTransformSettings
    {
        double defaultSplitRatio{ Workspaces::DefaultSplitRatio };
        double cornerWidthRatio{ 0.35 };
        double cornerHeightRatio{ 0.5 };
    };

    class LayoutTransformer
    {
    public:
        static LayoutTransformResult Insert(
            const LayoutNodePtr& targetRoot,
            std::optional<std::string_view> targetNodeId,
            const LayoutNodePtr& sourceRoot,
            DockZone zone,
            std::string emptySlotId,
            const LayoutTransformSettings& settings = {});

        static LayoutTransformResult MoveWithin(
            const LayoutNodePtr& root,
            std::string_view sourceNodeId,
            std::string_view targetNodeId,
            DockZone zone,
            std::string emptySlotId,
            std::string fallbackSlotId = {},
            const LayoutTransformSettings& settings = {});

        static LayoutTransformResult Remove(
            const LayoutNodePtr& root,
            std::string_view sourceNodeId,
            std::string fallbackSlotId = {});

        static LayoutTransformResult FillEmptySlot(
            const LayoutNodePtr& root,
            std::string_view slotId,
            const LayoutNodePtr& sourceRoot);

        static DockingPlan BuildProposedLayout(
            DockSource source,
            DockTarget target,
            DockZone zone,
            const LayoutNodePtr& sourceLayout,
            const LayoutNodePtr& targetLayout,
            std::string emptySlotId,
            const DockingCapabilities& capabilities,
            bool sameProcess,
            const LayoutTransformSettings& settings = {});
    };
}
