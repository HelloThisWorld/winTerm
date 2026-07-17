// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../../winterm/Docking/Drag/DockDragSession.h"
#include "../../winterm/Docking/History/LayoutHistory.h"
#include "../../winterm/Docking/Layout/LayoutTransformer.h"
#include "../../winterm/Docking/Sessions/SessionOwnership.h"
#include "../../winterm/Docking/Transactions/LayoutTransaction.h"

using namespace WEX::TestExecution;
using namespace winTerm::Docking;
using namespace winTerm::Workspaces;

namespace
{
    SessionIdentity Identity(const std::string& id)
    {
        return { id, "conpty", "instance-a", true, true };
    }

    SessionOwner Owner(const std::string& window, const std::string& tab, const std::string& pane)
    {
        return { window, tab, pane };
    }

    DockingPlan ReadyPlan()
    {
        DockingPlan plan;
        plan.status = DockingStatus::Ready;
        plan.zone = DockZone::Right;
        plan.proposedSourceLayout = LayoutNodeDescriptor::EmptySlot("source-slot");
        plan.proposedTargetLayout = LayoutNodeDescriptor::Split(
            SplitOrientation::Vertical,
            0.5,
            LayoutNodeDescriptor::Pane("target-pane"),
            LayoutNodeDescriptor::Pane("source-pane"));
        plan.movedPaneIds = { "source-pane" };
        return plan;
    }
}

namespace SettingsModelUnitTests
{
    class WinTermDockingSafetyTests
    {
        TEST_CLASS(WinTermDockingSafetyTests);

        TEST_METHOD(SessionLeasePreventsPrematureRemoval);
        TEST_METHOD(SessionTransferRejectsAnotherProcess);
        TEST_METHOD(TransactionCommitsTargetBeforeSourceRelease);
        TEST_METHOD(TransactionFailureRestoresOwnership);
        TEST_METHOD(DragStateMachineRejectsIllegalTransitions);
        TEST_METHOD(DragTokenIsOpaqueExpiringAndSingleUse);
        TEST_METHOD(LayoutHistoryUndoRedoHonorsSessionLifetime);
    };

    void WinTermDockingSafetyTests::SessionLeasePreventsPrematureRemoval()
    {
        SessionOwnershipRegistry ownership;
        VERIFY_IS_TRUE(ownership.Register(Identity("session-1"), Owner("window-1", "tab-1", "pane-1")));
        {
            auto lease = ownership.AcquireLease({ "session-1" });
            VERIFY_IS_TRUE(static_cast<bool>(lease));
            VERIFY_ARE_EQUAL(size_t{ 1 }, ownership.LeaseCount("session-1"));
            VERIFY_IS_FALSE(ownership.Unregister("session-1"));
        }
        VERIFY_ARE_EQUAL(size_t{ 0 }, ownership.LeaseCount("session-1"));
        VERIFY_IS_TRUE(ownership.Unregister("session-1"));
    }

    void WinTermDockingSafetyTests::SessionTransferRejectsAnotherProcess()
    {
        SessionOwnershipRegistry ownership;
        VERIFY_IS_TRUE(ownership.Register(Identity("session-1"), Owner("window-1", "tab-1", "pane-1")));
        VERIFY_IS_TRUE(ownership.CanTransfer("session-1", "instance-a"));
        VERIFY_IS_FALSE(ownership.CanTransfer("session-1", "instance-b"));
    }

    void WinTermDockingSafetyTests::TransactionCommitsTargetBeforeSourceRelease()
    {
        SessionOwnershipRegistry ownership;
        VERIFY_IS_TRUE(ownership.Register(Identity("session-1"), Owner("window-1", "tab-1", "source-pane")));
        LayoutTransactionCoordinator coordinator{ ownership };
        std::vector<std::string> order;
        bool dirty = false;
        bool tokenInvalidated = false;

        LayoutTransactionRequest request;
        request.transactionId = "transaction-1";
        request.plan = ReadyPlan();
        request.snapshot.sourceLayout = LayoutNodeDescriptor::Pane("source-pane");
        request.snapshot.targetLayout = LayoutNodeDescriptor::Pane("target-pane");
        request.sessionIds = { "session-1" };
        request.targetOwners = { Owner("window-2", "tab-2", "source-pane") };
        request.callbacks.sourceAvailable = [] { return true; };
        request.callbacks.targetAvailable = [] { return true; };
        request.callbacks.prepareTarget = [&](const auto&) {
            order.emplace_back("prepare");
            return true;
        };
        request.callbacks.commitModel = [&](const auto&) {
            order.emplace_back("model");
            return true;
        };
        request.callbacks.commitVisualTree = [&](const auto&) {
            order.emplace_back("visual");
            return true;
        };
        request.callbacks.rollback = [](const auto&) { return true; };
        request.callbacks.markWorkspaceDirty = [&] {
            order.emplace_back("dirty");
            dirty = true;
            return true;
        };
        request.callbacks.invalidateDragToken = [&] { tokenInvalidated = true; };

        const auto result = coordinator.Execute(std::move(request));
        VERIFY_ARE_EQUAL(static_cast<int>(DockingStatus::Committed), static_cast<int>(result.docking.status));
        VERIFY_ARE_EQUAL(size_t{ 4 }, order.size());
        VERIFY_ARE_EQUAL(std::string{ "prepare" }, order[0]);
        VERIFY_ARE_EQUAL(std::string{ "model" }, order[1]);
        VERIFY_ARE_EQUAL(std::string{ "visual" }, order[2]);
        VERIFY_ARE_EQUAL(std::string{ "dirty" }, order[3]);
        VERIFY_IS_TRUE(dirty);
        VERIFY_IS_TRUE(tokenInvalidated);
        VERIFY_IS_TRUE(ownership.Owner("session-1") == std::optional{ Owner("window-2", "tab-2", "source-pane") });
        VERIFY_ARE_EQUAL(size_t{ 0 }, ownership.LeaseCount("session-1"));
    }

    void WinTermDockingSafetyTests::TransactionFailureRestoresOwnership()
    {
        SessionOwnershipRegistry ownership;
        const auto originalOwner = Owner("window-1", "tab-1", "source-pane");
        VERIFY_IS_TRUE(ownership.Register(Identity("session-1"), originalOwner));
        LayoutTransactionCoordinator coordinator{ ownership };
        bool rollbackCalled = false;

        LayoutTransactionRequest request;
        request.transactionId = "transaction-rollback";
        request.plan = ReadyPlan();
        request.snapshot.sourceLayout = LayoutNodeDescriptor::Pane("source-pane");
        request.snapshot.targetLayout = LayoutNodeDescriptor::Pane("target-pane");
        request.sessionIds = { "session-1" };
        request.targetOwners = { Owner("window-2", "tab-2", "source-pane") };
        request.callbacks.prepareTarget = [](const auto&) { return true; };
        request.callbacks.commitModel = [](const auto&) { return true; };
        request.callbacks.commitVisualTree = [](const auto&) { return false; };
        request.callbacks.rollback = [&](const auto&) {
            rollbackCalled = true;
            return true;
        };

        const auto result = coordinator.Execute(std::move(request));
        VERIFY_ARE_EQUAL(static_cast<int>(DockingStatus::RolledBack), static_cast<int>(result.docking.status));
        VERIFY_IS_TRUE(result.ownershipRestored);
        VERIFY_IS_TRUE(result.layoutRestored);
        VERIFY_IS_TRUE(rollbackCalled);
        VERIFY_IS_TRUE(ownership.Owner("session-1") == std::optional{ originalOwner });
        VERIFY_ARE_EQUAL(size_t{ 0 }, ownership.LeaseCount("session-1"));
    }

    void WinTermDockingSafetyTests::DragStateMachineRejectsIllegalTransitions()
    {
        DockDragStateMachine state;
        VERIFY_IS_FALSE(state.Transition(DockDragState::Committing));
        VERIFY_IS_TRUE(state.Transition(DockDragState::PointerPressed));
        VERIFY_IS_TRUE(state.Transition(DockDragState::DragPending));
        VERIFY_IS_TRUE(state.Transition(DockDragState::Dragging));
        VERIFY_IS_TRUE(state.Transition(DockDragState::TargetAcquired));
        VERIFY_IS_TRUE(state.Transition(DockDragState::DropPending));
        VERIFY_IS_TRUE(state.Transition(DockDragState::Committing));
        VERIFY_IS_TRUE(state.Fail("target failed"));
        VERIFY_IS_TRUE(state.Cancel(DragCancellationReason::CommitFailed));
        VERIFY_IS_TRUE(state.Reset());
    }

    void WinTermDockingSafetyTests::DragTokenIsOpaqueExpiringAndSingleUse()
    {
        DragPayloadRegistry registry{ std::chrono::seconds{ 1 } };
        DockSource source;
        source.type = DockSourceType::Pane;
        source.windowId = "window-1";
        source.tabId = "tab-1";
        source.paneId = "pane-1";
        const auto now = std::chrono::steady_clock::now();
        const auto token = registry.Register("instance-a", source, DragCapability::Pane, now);
        VERIFY_IS_FALSE(token.empty());
        VERIFY_IS_TRUE(token.find("pane-1") == std::string::npos);
        VERIFY_IS_FALSE(registry.Resolve("forged", "instance-a", DragCapability::Pane, now).has_value());
        VERIFY_IS_FALSE(registry.Resolve(token, "instance-b", DragCapability::Pane, now).has_value());
        VERIFY_IS_TRUE(registry.Resolve(token, "instance-a", DragCapability::Pane, now).has_value());
        VERIFY_IS_TRUE(registry.Consume(token, "instance-a", DragCapability::Pane, now).has_value());
        VERIFY_IS_FALSE(registry.Resolve(token, "instance-a", DragCapability::Pane, now).has_value());

        const auto expired = registry.Register("instance-a", source, DragCapability::Pane, now);
        VERIFY_IS_FALSE(registry.Resolve(expired, "instance-a", DragCapability::Pane, now + std::chrono::seconds{ 2 }).has_value());
    }

    void WinTermDockingSafetyTests::LayoutHistoryUndoRedoHonorsSessionLifetime()
    {
        LayoutHistory history{ 2 };
        LayoutHistoryEntry entry;
        entry.transactionId = "transaction-1";
        entry.sessionIds = { "session-1" };
        entry.before.sourceLayout = LayoutNodeDescriptor::Pane("pane-1");
        entry.after.targetLayout = LayoutNodeDescriptor::Pane("pane-1");
        VERIFY_IS_TRUE(history.Record(entry));

        bool appliedBefore = false;
        const auto available = [](const auto&) { return true; };
        const auto undo = history.Undo(available, [&](const auto&, const bool useBefore) {
            appliedBefore = useBefore;
            return true;
        });
        VERIFY_IS_TRUE(undo.Succeeded());
        VERIFY_IS_TRUE(appliedBefore);
        VERIFY_ARE_EQUAL(size_t{ 1 }, history.RedoCount());

        const auto redo = history.Redo(available, [](const auto&, const bool useBefore) {
            return !useBefore;
        });
        VERIFY_IS_TRUE(redo.Succeeded());

        VERIFY_IS_TRUE(history.Record(entry));
        VERIFY_ARE_EQUAL(size_t{ 0 }, history.RedoCount());
        VERIFY_IS_FALSE(history.CanUndo([](const auto&) { return false; }));
    }
}
