// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../../Workspaces/Model/WorkspaceDescriptor.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace winTerm::Docking
{
    enum class DockSourceType
    {
        Tab,
        Pane,
        LayoutSubtree,
    };

    enum class DockTargetType
    {
        TabStrip,
        WindowContent,
        Pane,
        EmptySlot,
        NewWindow,
    };

    enum class DockZone
    {
        Center,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
    };

    enum class DockOperation
    {
        Move,
    };

    enum class DockingStatus
    {
        Ready,
        Invalid,
        Committed,
        RolledBack,
        Cancelled,
        Failed,
    };

    struct DockSource
    {
        DockSourceType type{ DockSourceType::Pane };
        std::string windowId;
        std::string tabId;
        std::optional<std::string> paneId;
        std::shared_ptr<Workspaces::LayoutNodeDescriptor> root;
        std::vector<std::string> paneIds;
        std::optional<std::string> activePaneId;
        std::string title;
    };

    struct DockTarget
    {
        DockTargetType type{ DockTargetType::Pane };
        std::string windowId;
        std::optional<std::string> tabId;
        std::optional<std::string> nodeId;
        std::optional<size_t> tabIndex;
    };

    struct DockingCapabilities
    {
        bool canMove{ true };
        bool canTransferLiveTab{ true };
        bool canTransferLivePane{ true };
        bool canTransferAcrossProcess{ false };
        bool canDockEdges{ true };
        bool canDockCorners{ true };
        bool canUseEmptySlots{ true };
        bool targetClosing{ false };
        bool readOnlyRestricted{ false };
        size_t currentPaneCount{};
        size_t sourcePaneCount{};
        size_t maximumPaneCount{ Workspaces::MaximumWorkspacePanesPerTab };

        bool Supports(const DockSource& source, const DockTarget& target, DockZone zone, bool sameProcess) const noexcept;
        std::string DisabledReason(const DockSource& source, const DockTarget& target, DockZone zone, bool sameProcess) const;
    };

    struct DockingPlan
    {
        DockSource source;
        DockTarget target;
        DockZone zone{ DockZone::Center };
        DockOperation operation{ DockOperation::Move };
        std::shared_ptr<Workspaces::LayoutNodeDescriptor> proposedSourceLayout;
        std::shared_ptr<Workspaces::LayoutNodeDescriptor> proposedTargetLayout;
        std::vector<std::string> movedPaneIds;
        DockingStatus status{ DockingStatus::Invalid };
        std::string invalidReason;
    };

    struct DockingResult
    {
        DockingStatus status{ DockingStatus::Invalid };
        std::string transactionId;
        std::string message;
        bool workspaceDirty{ false };

        bool Succeeded() const noexcept;
    };

    std::string_view ToString(DockSourceType value) noexcept;
    std::string_view ToString(DockTargetType value) noexcept;
    std::string_view ToString(DockZone value) noexcept;
    std::optional<DockZone> DockZoneFromString(std::string_view value) noexcept;
}
