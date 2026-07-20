// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../../winterm/Accessibility/KeyboardDockingController.h"
#include "../../winterm/Actions/DirectedSplitAction.h"
#include "../../winterm/Actions/MovePaneAction.h"
#include "../../winterm/Docking/Drag/PaneHandleDragSource.h"
#include "../../winterm/Docking/Layout/LayoutTree.h"
#include "../../winterm/PaneControls/PaneCommandMenu.h"
#include "../../winterm/PaneControls/PaneHeader.h"

using namespace WEX::TestExecution;
using namespace winTerm::Accessibility;
using namespace winTerm::Actions;
using namespace winTerm::Docking;
using namespace winTerm::PaneControls;
using namespace winTerm::Workspaces;

namespace
{
    LayoutNodePtr Pane(const std::string& id)
    {
        return LayoutNodeDescriptor::Pane(id);
    }

    DirectedSplitContext SplitContext(const DockZone direction)
    {
        DirectedSplitContext context;
        context.layout = LayoutNodeDescriptor::Split(
            SplitOrientation::Vertical,
            0.5,
            Pane("pane-a"),
            Pane("pane-b"));
        context.windowId = "window";
        context.tabId = "tab";
        context.focusedPaneId = "pane-b";
        context.newPaneId = "pane-new";
        context.direction = direction;
        context.tabBounds = { 0, 0, 1000, 600 };
        context.focusedPaneBounds = { 500, 0, 500, 600 };
        context.defaultProfileId = "default-profile";
        return context;
    }
}

namespace SettingsModelUnitTests
{
    class WinTermPaneControlsTests
    {
        TEST_CLASS(WinTermPaneControlsTests);

        TEST_METHOD(DirectedSplitPlacesNewPaneOnEverySide);
        TEST_METHOD(DirectedSplitTargetsFocusedNestedPane);
        TEST_METHOD(DirectedSplitResolvesProfileAndRejectsSmallPane);
        TEST_METHOD(PaneHeaderVisibilityTitleAndStatusAreAccessible);
        TEST_METHOD(PaneMenuUsesOneCapabilityAwareCommandModel);
        TEST_METHOD(PaneHandleDoesNotDragTerminalContent);
        TEST_METHOD(MovePanePlansPreserveThePaneNode);
        TEST_METHOD(PaneHandleDragUsesTransformerPreview);
        TEST_METHOD(KeyboardMoveModeCyclesTargets);
    };

    void WinTermPaneControlsTests::DirectedSplitPlacesNewPaneOnEverySide()
    {
        for (const auto direction : { DockZone::Top, DockZone::Bottom, DockZone::Left, DockZone::Right })
        {
            const auto result = DirectedSplitAction::BuildPlan(SplitContext(direction));
            VERIFY_IS_TRUE(result.Succeeded());
            const auto focusedBranch = result.proposedLayout->second;
            VERIFY_ARE_EQUAL(static_cast<int>(LayoutNodeType::Split), static_cast<int>(focusedBranch->type));
            const auto sourceFirst = direction == DockZone::Top || direction == DockZone::Left;
            VERIFY_ARE_EQUAL(
                std::string{ sourceFirst ? "pane-new" : "pane-b" },
                focusedBranch->first->paneId);
            VERIFY_ARE_EQUAL(
                std::string{ sourceFirst ? "pane-b" : "pane-new" },
                focusedBranch->second->paneId);
            const auto expectedOrientation =
                direction == DockZone::Top || direction == DockZone::Bottom ?
                    SplitOrientation::Horizontal :
                    SplitOrientation::Vertical;
            VERIFY_ARE_EQUAL(
                static_cast<int>(expectedOrientation),
                static_cast<int>(focusedBranch->orientation));
        }
    }

    void WinTermPaneControlsTests::DirectedSplitTargetsFocusedNestedPane()
    {
        auto context = SplitContext(DockZone::Top);
        const auto result = DirectedSplitAction::BuildPlan(context);
        VERIFY_IS_TRUE(result.Succeeded());
        VERIFY_ARE_EQUAL(std::string{ "pane-a" }, result.proposedLayout->first->paneId);
        VERIFY_ARE_EQUAL(static_cast<int>(LayoutNodeType::Split), static_cast<int>(result.proposedLayout->second->type));
        VERIFY_ARE_EQUAL(size_t{ 3 }, LayoutTree{ result.proposedLayout }.PaneCount());
    }

    void WinTermPaneControlsTests::DirectedSplitResolvesProfileAndRejectsSmallPane()
    {
        auto selected = SplitContext(DockZone::Right);
        selected.profilePolicy = SplitProfilePolicy::AskEveryTime;
        selected.selectedProfileId = "selected-profile";
        const auto selectedResult = DirectedSplitAction::BuildPlan(selected);
        VERIFY_IS_TRUE(selectedResult.Succeeded());
        VERIFY_ARE_EQUAL(std::string{ "selected-profile" }, selectedResult.profileId);

        auto tooSmall = SplitContext(DockZone::Left);
        tooSmall.focusedPaneBounds.width = 200;
        const auto rejected = DirectedSplitAction::BuildPlan(tooSmall);
        VERIFY_IS_FALSE(rejected.Succeeded());
        VERIFY_IS_TRUE(rejected.invalidReason.find("too narrow") != std::string::npos);
        VERIFY_ARE_EQUAL(size_t{ 2 }, LayoutTree{ tooSmall.layout }.PaneCount());
    }

    void WinTermPaneControlsTests::PaneHeaderVisibilityTitleAndStatusAreAccessible()
    {
        PaneHeaderSettings settings;
        PaneHeader header{ settings };
        PaneHeaderState state;
        state.paneCount = 2;
        state.focused = true;
        state.readOnly = true;
        state.execution = PaneExecutionState::Running;
        state.titles.shellTitle = "C:\\Users\\private\\project";
        const auto presentation = header.Present(state);
        VERIFY_IS_TRUE(presentation.visible);
        VERIFY_ARE_EQUAL(std::string{ "project" }, presentation.title);
        VERIFY_IS_TRUE(presentation.accessibleName.find("Focused") != std::string::npos);
        VERIFY_IS_TRUE(presentation.accessibleName.find("Running command") != std::string::npos);
        VERIFY_IS_TRUE(presentation.accessibleName.find("Read-only") != std::string::npos);

        settings.visibility = PaneHeaderVisibility::Never;
        header.Settings(settings);
        VERIFY_IS_FALSE(header.Present(state).visible);
        VERIFY_IS_TRUE(settings.HasKeyboardAlternative(true, true));

        state.titles.shellTitle =
            "C:\\private\\"
            "a-very-long-pane-title-that-must-be-truncated-visually-but-remain-complete-for-accessibility";
        settings.visibility = PaneHeaderVisibility::Always;
        header.Settings(settings);
        const auto longTitle = header.Present(state);
        VERIFY_IS_TRUE(longTitle.title.size() <= size_t{ 80 });
        VERIFY_IS_TRUE(
            longTitle.accessibleName.find("remain-complete-for-accessibility") != std::string::npos);
    }

    void WinTermPaneControlsTests::PaneMenuUsesOneCapabilityAwareCommandModel()
    {
        PaneCommandContext context;
        context.paneCount = 1;
        context.focusedPaneBounds = { 0, 0, 1000, 600 };
        context.livePaneTransferSupported = false;
        const auto rightClick = PaneCommandMenu::Build(context, PaneMenuInvocation::HandleRightClick);
        const auto overflow = PaneCommandMenu::Build(context, PaneMenuInvocation::OverflowButton);
        VERIFY_ARE_EQUAL(rightClick.size(), overflow.size());
        VERIFY_IS_TRUE(PaneCommandModel::Find(rightClick, "splitPaneTop") != nullptr);
        VERIFY_IS_TRUE(PaneCommandModel::Find(rightClick, "splitPaneBottom") != nullptr);
        VERIFY_IS_TRUE(PaneCommandModel::Find(rightClick, "splitPaneLeft") != nullptr);
        VERIFY_IS_TRUE(PaneCommandModel::Find(rightClick, "splitPaneRight") != nullptr);
        VERIFY_IS_FALSE(PaneCommandModel::Find(rightClick, "movePaneToNewTab")->enabled);
        VERIFY_IS_FALSE(PaneCommandModel::Find(rightClick, "movePaneToNewWindow")->enabled);
        VERIFY_IS_TRUE(PaneCommandModel::Find(rightClick, "closeFocusedPane")->enabled);
    }

    void WinTermPaneControlsTests::PaneHandleDoesNotDragTerminalContent()
    {
        size_t menuOpenCount{};
        PaneHandle handle{ [&](const auto) { ++menuOpenCount; } };
        VERIFY_IS_TRUE(handle.CanStartDrag(PanePointerRegion::DragGrip, true));
        VERIFY_IS_FALSE(handle.CanStartDrag(PanePointerRegion::TerminalContent, true));
        VERIFY_IS_FALSE(handle.CanStartDrag(PanePointerRegion::Scrollbar, true));
        VERIFY_IS_TRUE(handle.HandleRightClick(PanePointerRegion::DragGrip));
        VERIFY_IS_TRUE(handle.HandlePrimaryClick(PanePointerRegion::OverflowButton));
        VERIFY_ARE_EQUAL(size_t{ 2 }, menuOpenCount);
    }

    void WinTermPaneControlsTests::MovePanePlansPreserveThePaneNode()
    {
        MovePaneContext context;
        context.sourceLayout = LayoutNodeDescriptor::Split(
            SplitOrientation::Vertical,
            0.5,
            Pane("pane-source"),
            Pane("pane-remaining"));
        context.sourceWindowId = "window";
        context.sourceTabId = "tab";
        context.sourcePaneId = "pane-source";
        context.targetWindowId = "window-new";
        context.capabilities.sourcePaneCount = 1;

        const auto tabPlan = MovePaneAction::ToNewTab(context);
        VERIFY_IS_TRUE(tabPlan.Succeeded());
        VERIFY_ARE_EQUAL(std::string{ "pane-source" }, tabPlan.targetRoot->paneId);
        VERIFY_ARE_EQUAL(std::string{ "pane-remaining" }, tabPlan.sourceAfter->paneId);

        const auto windowPlan = MovePaneAction::ToNewWindow(context);
        VERIFY_IS_TRUE(windowPlan.Succeeded());
        VERIFY_ARE_EQUAL(std::string{ "pane-source" }, windowPlan.targetRoot->paneId);
        VERIFY_ARE_EQUAL(size_t{ 1 }, windowPlan.sessionPaneIds.size());

        context.sameProcess = false;
        VERIFY_IS_FALSE(MovePaneAction::ToNewWindow(context).Succeeded());
    }

    void WinTermPaneControlsTests::PaneHandleDragUsesTransformerPreview()
    {
        DragPayloadRegistry registry;
        PaneHandleDragSource drag{ registry };
        PaneHandleDragContext context;
        context.processInstanceId = "process";
        context.source.type = DockSourceType::Pane;
        context.source.windowId = "window";
        context.source.tabId = "tab";
        context.source.paneId = "pane-source";
        context.sourcePaneLayout = Pane("pane-source");
        context.sourceTabLayout = LayoutNodeDescriptor::Split(
            SplitOrientation::Vertical,
            0.5,
            Pane("pane-source"),
            Pane("pane-target"));
        context.pressedPoint = { 0, 0 };
        VERIFY_IS_TRUE(drag.PointerPressed(PanePointerRegion::DragGrip, context));
        VERIFY_IS_TRUE(drag.PointerMoved({ 10, 0 }, std::chrono::milliseconds{ 20 }));

        PaneHandleDragPreviewRequest request;
        request.target.type = DockTargetType::Pane;
        request.target.windowId = "window";
        request.target.tabId = "tab";
        request.target.nodeId = "pane-target";
        request.zone = DockZone::Top;
        request.targetLayout = context.sourceTabLayout;
        request.capabilities.sourcePaneCount = 1;
        request.targetBounds = { 0, 0, 1000, 600 };
        const auto preview = drag.Preview(request);
        VERIFY_IS_TRUE(preview.Succeeded());
        VERIFY_ARE_EQUAL(size_t{ 2 }, LayoutTree{ preview.plan.proposedTargetLayout }.PaneCount());
        VERIFY_IS_TRUE(drag.RequestDrop());
        VERIFY_IS_TRUE(drag.BeginCommit());
        VERIFY_IS_TRUE(drag.Complete());
        VERIFY_IS_TRUE(drag.Token().empty());
    }

    void WinTermPaneControlsTests::KeyboardMoveModeCyclesTargets()
    {
        KeyboardDockingController controller;
        const std::vector targets{
            KeyboardDockingTarget{ "one", "Command Prompt pane", { DockZone::Top, DockZone::Bottom } },
            KeyboardDockingTarget{ "two", "PowerShell pane", { DockZone::Left, DockZone::Right } },
        };
        VERIFY_IS_TRUE(controller.StartMoveMode("PowerShell pane", targets, 0, DockZone::Top));
        VERIFY_IS_TRUE(controller.Announcement().find("top of Command Prompt pane") != std::string::npos);
        VERIFY_IS_TRUE(controller.Handle(DockingNavigationKey::Tab));
        VERIFY_ARE_EQUAL(size_t{ 1 }, controller.SelectedTarget().value());
        VERIFY_IS_TRUE(controller.Handle(DockingNavigationKey::Right));
        VERIFY_IS_TRUE(controller.Handle(DockingNavigationKey::Enter));
        VERIFY_IS_TRUE(controller.Announcement().find("Move requested") != std::string::npos);
        VERIFY_ARE_EQUAL(size_t{ 26 }, KeyboardDockingController::Commands().size());
    }
}
