// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PaneCommandModel.h"

#include <array>

using namespace winTerm::Actions;
using namespace winTerm::Docking;
using namespace winTerm::PaneControls;

std::vector<PaneCommandDescriptor> PaneCommandModel::Build(const PaneCommandContext& context)
{
    std::vector<PaneCommandDescriptor> result;
    result.reserve(12);

    const std::array directions{
        std::pair{ PaneCommand::SplitTop, DockZone::Top },
        std::pair{ PaneCommand::SplitBottom, DockZone::Bottom },
        std::pair{ PaneCommand::SplitLeft, DockZone::Left },
        std::pair{ PaneCommand::SplitRight, DockZone::Right },
    };
    for (const auto [command, direction] : directions)
    {
        std::string disabledReason;
        const auto canSplit = DirectedSplitAction::CanSplit(
            direction,
            context.focusedPaneBounds,
            context.geometrySettings,
            &disabledReason);
        result.emplace_back(PaneCommandDescriptor{
            command,
            std::string{ DirectedSplitAction::CommandId(direction) },
            "split",
            std::string{ ToString(direction) },
            std::string{ DirectedSplitAction::AccessibleName(direction) },
            std::string{ ToString(direction) },
            canSplit && !context.transactionProtected,
            context.transactionProtected ? "The pane is protected by an active layout transaction." : disabledReason,
            direction,
        });
    }

    result.emplace_back(PaneCommandDescriptor{
        PaneCommand::MoveToNewTab,
        "movePaneToNewTab",
        "move",
        "Move to New Tab",
        "Move pane to a new tab",
        "newTab",
        context.paneCount > 1 && !context.transactionProtected,
        context.transactionProtected ?
            "The pane is protected by an active layout transaction." :
            context.paneCount <= 1 ? "This pane is already the only pane in its tab." : std::string{},
    });
    result.emplace_back(PaneCommandDescriptor{
        PaneCommand::MoveToNewWindow,
        "movePaneToNewWindow",
        "move",
        "Move to New Window",
        "Move pane to a new window",
        "newWindow",
        context.livePaneTransferSupported &&
            context.sameProcessWindowHosting &&
            !context.transactionProtected,
        context.transactionProtected ?
            "The pane is protected by an active layout transaction." :
            !context.livePaneTransferSupported || !context.sameProcessWindowHosting ?
                "Live pane transfer is not supported by this window host." :
                std::string{},
    });
    result.emplace_back(PaneCommandDescriptor{
        PaneCommand::ClosePane,
        "closeFocusedPane",
        {},
        "Close Pane",
        "Close pane",
        "close",
        !context.transactionProtected,
        context.transactionProtected ? "The pane is protected by an active layout transaction." : std::string{},
    });
    result.emplace_back(PaneCommandDescriptor{ PaneCommand::FocusPane, "focusPane", "more", "Focus Pane", "Focus pane", "focus" });
    result.emplace_back(PaneCommandDescriptor{ PaneCommand::ZoomPane, "zoomPane", "more", "Zoom Pane", "Zoom pane", "zoom" });
    result.emplace_back(PaneCommandDescriptor{ PaneCommand::PaneSettings, "paneSettings", "more", "Pane Settings", "Open pane settings", "settings" });
    result.emplace_back(PaneCommandDescriptor{ PaneCommand::StartMoveMode, "startPaneMoveMode", {}, "Move Pane", "Start pane move mode", "move" });
    result.emplace_back(PaneCommandDescriptor{ PaneCommand::OpenPaneMenu, "openPaneMenu", {}, "Open Pane Menu", "Open pane menu", "more" });
    return result;
}

const PaneCommandDescriptor* PaneCommandModel::Find(
    const std::vector<PaneCommandDescriptor>& commands,
    const std::string_view id) noexcept
{
    for (const auto& command : commands)
    {
        if (command.id == id)
        {
            return &command;
        }
    }
    return nullptr;
}
