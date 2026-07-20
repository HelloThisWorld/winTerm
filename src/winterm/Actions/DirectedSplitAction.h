// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Docking/Layout/LayoutGeometry.h"
#include "../Docking/Layout/LayoutTransformer.h"

#include <optional>
#include <string>
#include <string_view>

namespace winTerm::Actions
{
    enum class SplitProfilePolicy
    {
        DefaultProfile,
        CurrentPaneProfile,
        AskEveryTime,
    };

    struct DirectedSplitContext
    {
        Docking::LayoutNodePtr layout;
        std::string windowId;
        std::string tabId;
        std::string focusedPaneId;
        std::string newPaneId;
        Docking::DockZone direction{ Docking::DockZone::Center };
        Docking::LayoutRect tabBounds;
        Docking::LayoutRect focusedPaneBounds;
        Docking::LayoutGeometrySettings geometrySettings;
        double splitRatio{ Workspaces::DefaultSplitRatio };
        SplitProfilePolicy profilePolicy{ SplitProfilePolicy::DefaultProfile };
        std::string defaultProfileId;
        std::string currentPaneProfileId;
        std::optional<std::string> selectedProfileId;
        bool focusNewPane{ true };
    };

    struct DirectedSplitPlan
    {
        bool valid{ false };
        std::string invalidReason;
        std::string profileId;
        Docking::DockingPlan docking;
        Docking::LayoutNodePtr proposedLayout;
        std::string historyDescription;
        bool focusNewPane{ true };

        bool Succeeded() const noexcept;
    };

    class DirectedSplitAction
    {
    public:
        static DirectedSplitPlan BuildPlan(const DirectedSplitContext& context);
        static bool IsDirection(Docking::DockZone direction) noexcept;
        static bool CanSplit(
            Docking::DockZone direction,
            Docking::LayoutRect focusedPaneBounds,
            const Docking::LayoutGeometrySettings& geometrySettings,
            std::string* disabledReason = nullptr) noexcept;
        static std::string_view CommandId(Docking::DockZone direction) noexcept;
        static std::string_view AccessibleName(Docking::DockZone direction) noexcept;
    };
}
