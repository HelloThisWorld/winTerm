// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Actions/DirectedSplitAction.h"
#include "../Docking/Model/DockingModel.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace winTerm::PaneControls
{
    enum class PaneCommand
    {
        SplitTop,
        SplitBottom,
        SplitLeft,
        SplitRight,
        BalancePanes,
        ClosePane,
        FocusPane,
        ZoomPane,
        PaneSettings,
        OpenPaneMenu,
    };

    struct PaneCommandContext
    {
        size_t paneCount{ 1 };
        bool transactionProtected{ false };
        bool owningSplitCanBalance{ false };
        Docking::LayoutRect focusedPaneBounds;
        Docking::LayoutGeometrySettings geometrySettings;
    };

    struct PaneCommandDescriptor
    {
        PaneCommand command{ PaneCommand::OpenPaneMenu };
        std::string id;
        std::string parentId;
        std::string label;
        std::string accessibleName;
        std::string icon;
        bool enabled{ true };
        std::string disabledReason;
        std::optional<Docking::DockZone> direction;
    };

    class PaneCommandModel
    {
    public:
        static std::vector<PaneCommandDescriptor> Build(const PaneCommandContext& context);
        static const PaneCommandDescriptor* Find(
            const std::vector<PaneCommandDescriptor>& commands,
            std::string_view id) noexcept;
    };
}
