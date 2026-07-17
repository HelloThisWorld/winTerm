// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../../winterm/Docking/Diagnostics/DockingDiagnostics.h"

using namespace WEX::TestExecution;
using namespace winTerm::Docking;
using namespace winTerm::Workspaces;

namespace SettingsModelUnitTests
{
    class WinTermDockingDiagnosticsTests
    {
        TEST_CLASS(WinTermDockingDiagnosticsTests);

        TEST_METHOD(LayoutHealthCountsEmptySlotsAndOrphans);
        TEST_METHOD(TraceIsBoundedAndRedacted);
    };

    void WinTermDockingDiagnosticsTests::LayoutHealthCountsEmptySlotsAndOrphans()
    {
        WorkspaceDescriptor workspace;
        WindowDescriptor window;
        TabDescriptor tab;
        PaneDescriptor livePane;
        livePane.id = "pane-live";
        tab.panes.emplace_back(std::move(livePane));
        PaneDescriptor orphanPane;
        orphanPane.id = "pane-orphan-session";
        tab.panes.emplace_back(std::move(orphanPane));
        tab.layout = LayoutNodeDescriptor::Split(
            SplitOrientation::Vertical,
            0.35,
            LayoutNodeDescriptor::Pane("pane-live"),
            LayoutNodeDescriptor::EmptySlot("slot-1"));
        window.tabs.emplace_back(std::move(tab));
        workspace.windows.emplace_back(std::move(window));

        const auto report = DockingDiagnostics::InspectLayout(workspace);
        VERIFY_ARE_EQUAL(size_t{ 3 }, report.nodeCount);
        VERIFY_ARE_EQUAL(size_t{ 1 }, report.paneCount);
        VERIFY_ARE_EQUAL(size_t{ 1 }, report.emptySlotCount);
        VERIFY_ARE_EQUAL(size_t{ 1 }, report.orphanSessionCount);
        VERIFY_IS_FALSE(report.IsValid());
    }

    void WinTermDockingDiagnosticsTests::TraceIsBoundedAndRedacted()
    {
        DockingDiagnostics diagnostics{ 1 };
        diagnostics.Record({ DockSourceType::Tab, DockTargetType::WindowContent, DockZone::Left, DockingStatus::Committed, 8, 3, "ok" });
        diagnostics.Record({ DockSourceType::Pane, DockTargetType::Pane, DockZone::Right, DockingStatus::Failed, 12, 5, "attach failed: C:\\Users\\secret" });

        const auto snapshot = diagnostics.Snapshot();
        VERIFY_ARE_EQUAL(size_t{ 1 }, snapshot.size());
        VERIFY_ARE_EQUAL(std::string{ "other" }, snapshot.front().errorCategory);
        const auto text = diagnostics.CopySafeText();
        VERIFY_IS_TRUE(text.find("secret") == std::string::npos);
        VERIFY_IS_TRUE(text.find("\\") == std::string::npos);
        diagnostics.Clear();
        VERIFY_ARE_EQUAL(size_t{ 0 }, diagnostics.Snapshot().size());
    }
}
