// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Layout/LayoutValidator.h"
#include "../Model/DockingModel.h"
#include "../Sessions/SessionOwnership.h"

#include <functional>
#include <string>
#include <vector>

namespace winTerm::Docking
{
    enum class LayoutTransactionState
    {
        Created,
        Validated,
        SessionsLeased,
        TargetPrepared,
        OwnershipReserved,
        ModelCommitted,
        VisualCommitted,
        Completed,
        RollingBack,
        RolledBack,
        Failed,
    };

    struct FocusSnapshot
    {
        std::string activeWindowId;
        std::string activeTabId;
        std::string activePaneId;
        std::optional<std::string> zoomedPaneId;
    };

    struct LayoutTransactionSnapshot
    {
        LayoutNodePtr sourceLayout;
        LayoutNodePtr targetLayout;
        FocusSnapshot focus;
        std::vector<SessionOwnershipSnapshot> sessionOwners;
    };

    struct LayoutTransactionCallbacks
    {
        std::function<bool()> sourceAvailable;
        std::function<bool()> targetAvailable;
        std::function<bool(const DockingPlan&)> prepareTarget;
        std::function<bool(const DockingPlan&)> commitModel;
        std::function<bool(const DockingPlan&)> commitVisualTree;
        std::function<bool(const LayoutTransactionSnapshot&)> rollback;
        std::function<bool(const LayoutTransactionSnapshot&)> recoverSessions;
        std::function<bool()> markWorkspaceDirty;
        std::function<void()> restoreFocus;
        std::function<void()> closeOverlay;
        std::function<void()> invalidateDragToken;
    };

    struct LayoutTransactionRequest
    {
        std::string transactionId;
        DockingPlan plan;
        LayoutTransactionSnapshot snapshot;
        std::vector<std::string> sessionIds;
        std::vector<SessionOwner> targetOwners;
        LayoutTransactionCallbacks callbacks;
    };

    struct LayoutTransactionResult
    {
        DockingResult docking;
        LayoutTransactionState state{ LayoutTransactionState::Created };
        bool ownershipRestored{ false };
        bool layoutRestored{ false };
        bool recoveryLayoutUsed{ false };
    };

    class LayoutTransactionCoordinator
    {
    public:
        explicit LayoutTransactionCoordinator(SessionOwnershipRegistry& ownership);

        LayoutTransactionResult Execute(LayoutTransactionRequest request);

    private:
        static bool _invoke(const std::function<bool()>& callback, bool defaultValue = true);
        static void _invoke(const std::function<void()>& callback) noexcept;

        SessionOwnershipRegistry& _ownership;
    };
}
