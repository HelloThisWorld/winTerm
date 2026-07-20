// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Docking/History/LayoutHistory.h"
#include "../Docking/Layout/LayoutTransformer.h"

#include <string>
#include <vector>

namespace winTerm::Actions
{
    enum class MovePaneDestination
    {
        NewTab,
        NewWindow,
    };

    struct MovePaneContext
    {
        Docking::LayoutNodePtr sourceLayout;
        std::string sourceWindowId;
        std::string sourceTabId;
        std::string sourcePaneId;
        std::string targetWindowId;
        std::string targetTabId;
        bool sameProcess{ true };
        Docking::DockingCapabilities capabilities;
    };

    struct MovePanePlan
    {
        bool valid{ false };
        std::string invalidReason;
        MovePaneDestination destination{ MovePaneDestination::NewTab };
        Docking::DockingPlan docking;
        Docking::LayoutNodePtr sourceAfter;
        Docking::LayoutNodePtr targetRoot;
        std::vector<std::string> sessionPaneIds;
        std::string historyDescription;

        bool Succeeded() const noexcept;
    };

    class MovePaneAction
    {
    public:
        static MovePanePlan ToNewTab(const MovePaneContext& context);
        static MovePanePlan ToNewWindow(const MovePaneContext& context);

    private:
        static MovePanePlan _build(const MovePaneContext& context, MovePaneDestination destination);
    };
}
