// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../../winterm/Actions/DirectedSplitAction.h"
#include "../../winterm/Docking/Layout/LayoutTree.h"
#include "../../winterm/PaneControls/PaneCommandMenu.h"
#include "../../winterm/PaneControls/PaneHeader.h"
#include "../../winterm/Settings/Docking/DockingSettingsModel.h"

using namespace WEX::TestExecution;
using namespace winTerm::Actions;
using namespace winTerm::Docking;
using namespace winTerm::PaneControls;
using namespace winTerm::Settings;
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
        TEST_METHOD(PaneHeaderUsesInformationIconSemantics);
        TEST_METHOD(PaneMenuContainsSplitBalanceAndNoMovementCommands);
        TEST_METHOD(PaneIconFocusesAndOpensUnifiedMenuWithoutDrag);
        TEST_METHOD(LayoutSettingsDefaultToCommonResizeSnapping);
    };

    void WinTermPaneControlsTests::DirectedSplitPlacesNewPaneOnEverySide()
    {
        for (const auto direction : { DockZone::Top, DockZone::Bottom, DockZone::Left, DockZone::Right })
        {
            const auto result = DirectedSplitAction::BuildPlan(SplitContext(direction));
            VERIFY_IS_TRUE(result.Succeeded());
            const auto focusedBranch = result.proposedLayout->second;
            VERIFY_ARE_EQUAL(static_cast<int>(LayoutNodeType::Split), static_cast<int>(focusedBranch->type));
            const auto newPaneFirst = direction == DockZone::Top || direction == DockZone::Left;
            VERIFY_ARE_EQUAL(
                std::string{ newPaneFirst ? "pane-new" : "pane-b" },
                focusedBranch->first->paneId);
            VERIFY_ARE_EQUAL(
                std::string{ newPaneFirst ? "pane-b" : "pane-new" },
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
        const auto result = DirectedSplitAction::BuildPlan(SplitContext(DockZone::Top));
        VERIFY_IS_TRUE(result.Succeeded());
        VERIFY_ARE_EQUAL(std::string{ "pane-a" }, result.proposedLayout->first->paneId);
        VERIFY_ARE_EQUAL(
            static_cast<int>(LayoutNodeType::Split),
            static_cast<int>(result.proposedLayout->second->type));
        VERIFY_ARE_EQUAL(size_t{ 3 }, LayoutTree{ result.proposedLayout }.PaneCount());
    }

    void WinTermPaneControlsTests::PaneHeaderUsesInformationIconSemantics()
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
        VERIFY_IS_TRUE(presentation.accessibleName.find("Move") == std::string::npos);

        settings.visibility = PaneHeaderVisibility::Never;
        header.Settings(settings);
        VERIFY_IS_FALSE(header.Present(state).visible);
        VERIFY_IS_TRUE(settings.HasKeyboardAlternative(true));
    }

    void WinTermPaneControlsTests::PaneMenuContainsSplitBalanceAndNoMovementCommands()
    {
        PaneCommandContext context;
        context.paneCount = 2;
        context.owningSplitCanBalance = true;
        context.focusedPaneBounds = { 0, 0, 1000, 600 };
        const auto iconMenu = PaneCommandMenu::Build(context, PaneMenuInvocation::IconRightClick);
        const auto overflowMenu = PaneCommandMenu::Build(context, PaneMenuInvocation::OverflowButton);
        VERIFY_ARE_EQUAL(iconMenu.size(), overflowMenu.size());
        VERIFY_IS_TRUE(PaneCommandModel::Find(iconMenu, "splitPaneTop") != nullptr);
        VERIFY_IS_TRUE(PaneCommandModel::Find(iconMenu, "splitPaneBottom") != nullptr);
        VERIFY_IS_TRUE(PaneCommandModel::Find(iconMenu, "splitPaneLeft") != nullptr);
        VERIFY_IS_TRUE(PaneCommandModel::Find(iconMenu, "splitPaneRight") != nullptr);
        VERIFY_IS_TRUE(PaneCommandModel::Find(iconMenu, "balancePanes")->enabled);
        VERIFY_IS_TRUE(PaneCommandModel::Find(iconMenu, "closeFocusedPane")->enabled);
        VERIFY_IS_TRUE(PaneCommandModel::Find(iconMenu, "movePaneToNewTab") == nullptr);
        VERIFY_IS_TRUE(PaneCommandModel::Find(iconMenu, "movePaneToNewWindow") == nullptr);
        VERIFY_IS_TRUE(PaneCommandModel::Find(iconMenu, "startPaneMoveMode") == nullptr);
    }

    void WinTermPaneControlsTests::PaneIconFocusesAndOpensUnifiedMenuWithoutDrag()
    {
        size_t menuOpenCount{};
        size_t focusCount{};
        PaneIcon icon{
            [&](const auto) { ++menuOpenCount; },
            [&] { ++focusCount; }
        };
        VERIFY_IS_TRUE(icon.HandlePrimaryClick(PanePointerRegion::PaneIcon));
        VERIFY_ARE_EQUAL(size_t{ 1 }, focusCount);
        VERIFY_IS_TRUE(icon.HandleRightClick(PanePointerRegion::PaneIcon));
        VERIFY_IS_TRUE(icon.HandlePrimaryClick(PanePointerRegion::OverflowButton));
        VERIFY_ARE_EQUAL(size_t{ 2 }, menuOpenCount);
        VERIFY_IS_FALSE(icon.HandlePrimaryClick(PanePointerRegion::TerminalContent));
        VERIFY_IS_FALSE(icon.HandleRightClick(PanePointerRegion::Scrollbar));
    }

    void WinTermPaneControlsTests::LayoutSettingsDefaultToCommonResizeSnapping()
    {
        DockingSettingsModel settings;
        VERIFY_IS_TRUE(settings.enableTabDragging);
        VERIFY_IS_TRUE(settings.allowTabTearOut);
        VERIFY_IS_TRUE(settings.includePaneResizeInHistory);
        VERIFY_IS_TRUE(settings.paneResizing.enableSnapping);
        VERIFY_ARE_EQUAL(
            static_cast<int>(winTerm::PaneResize::SnapPointPreset::CommonRatios),
            static_cast<int>(settings.paneResizing.preset));
        VERIFY_ARE_EQUAL(size_t{ 5 }, settings.paneResizing.Ratios().size());
        VERIFY_IS_TRUE(settings.Validate().empty());
    }
}
