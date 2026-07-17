// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../../winterm/Docking/Layout/LayoutGeometry.h"
#include "../../winterm/Docking/Layout/LayoutNormalizer.h"
#include "../../winterm/Docking/Layout/LayoutTransformer.h"
#include "../../winterm/Docking/Layout/LayoutValidator.h"
#include "../../winterm/Workspaces/Persistence/WorkspaceMigration.h"
#include "../../winterm/Workspaces/Persistence/WorkspaceSerializer.h"

using namespace WEX::TestExecution;
using namespace winTerm::Docking;
using namespace winTerm::Workspaces;

namespace
{
    LayoutNodePtr Pane(const std::string& id)
    {
        return LayoutNodeDescriptor::Pane(id);
    }

    LayoutNodePtr Target()
    {
        return Pane("target-pane");
    }

    LayoutNodePtr Source()
    {
        return Pane("source-pane");
    }

    WorkspaceDescriptor EmptyWorkspace()
    {
        WorkspaceDescriptor workspace;
        workspace.id = "workspace-empty";
        workspace.name = "Empty Workspace";
        workspace.createdAt = "2026-07-17T00:00:00Z";
        workspace.updatedAt = "2026-07-17T00:00:00Z";
        workspace.activeWindowId = "window-empty";

        WindowDescriptor window;
        window.id = "window-empty";
        window.monitor.workArea = { 0, 0, 1920, 1080 };
        window.bounds = { 100, 100, 1000, 700 };
        window.normalizedBounds = { 0.05, 0.05, 0.52, 0.65 };
        window.activeTabId = "tab-empty";

        TabDescriptor tab;
        tab.id = "tab-empty";
        tab.title = "Empty";
        tab.layout = LayoutNodeDescriptor::EmptySlot("slot-empty");
        window.tabs.emplace_back(std::move(tab));
        workspace.windows.emplace_back(std::move(window));
        return workspace;
    }
}

namespace SettingsModelUnitTests
{
    class WinTermDockingTests
    {
        TEST_CLASS(WinTermDockingTests);

        TEST_METHOD(EdgeDockingIsDeterministic);
        TEST_METHOD(CornerDockingCreatesExpectedEmptySlot);
        TEST_METHOD(RemovingPanePromotesSibling);
        TEST_METHOD(EmptySlotReplacementDoesNotAddSplit);
        TEST_METHOD(ValidatorRejectsDuplicatePaneAndSlotIds);
        TEST_METHOD(GeometryUsesTransformedLayout);
        TEST_METHOD(WorkspaceV2RoundTripsEmptySlot);
        TEST_METHOD(WorkspaceV1MigrationIsIdempotent);
    };

    void WinTermDockingTests::EdgeDockingIsDeterministic()
    {
        const auto left = LayoutTransformer::Insert(Target(), std::nullopt, Source(), DockZone::Left, {});
        VERIFY_IS_TRUE(left.Succeeded());
        VERIFY_ARE_EQUAL(static_cast<int>(LayoutNodeType::Split), static_cast<int>(left.root->type));
        VERIFY_ARE_EQUAL(static_cast<int>(SplitOrientation::Vertical), static_cast<int>(left.root->orientation));
        VERIFY_ARE_EQUAL(std::string{ "source-pane" }, left.root->first->paneId);
        VERIFY_ARE_EQUAL(std::string{ "target-pane" }, left.root->second->paneId);

        const auto bottom = LayoutTransformer::Insert(Target(), std::nullopt, Source(), DockZone::Bottom, {});
        VERIFY_IS_TRUE(bottom.Succeeded());
        VERIFY_ARE_EQUAL(static_cast<int>(SplitOrientation::Horizontal), static_cast<int>(bottom.root->orientation));
        VERIFY_ARE_EQUAL(std::string{ "target-pane" }, bottom.root->first->paneId);
        VERIFY_ARE_EQUAL(std::string{ "source-pane" }, bottom.root->second->paneId);
    }

    void WinTermDockingTests::CornerDockingCreatesExpectedEmptySlot()
    {
        const auto topLeft = LayoutTransformer::Insert(Target(), std::nullopt, Source(), DockZone::TopLeft, "slot-top-left");
        VERIFY_IS_TRUE(topLeft.Succeeded());
        VERIFY_ARE_EQUAL(static_cast<int>(SplitOrientation::Vertical), static_cast<int>(topLeft.root->orientation));
        VERIFY_ARE_EQUAL(static_cast<int>(LayoutNodeType::Split), static_cast<int>(topLeft.root->first->type));
        VERIFY_ARE_EQUAL(std::string{ "source-pane" }, topLeft.root->first->first->paneId);
        VERIFY_ARE_EQUAL(std::string{ "slot-top-left" }, topLeft.root->first->second->slotId);
        VERIFY_ARE_EQUAL(std::string{ "target-pane" }, topLeft.root->second->paneId);

        const auto bottomRight = LayoutTransformer::Insert(Target(), std::nullopt, Source(), DockZone::BottomRight, "slot-bottom-right");
        VERIFY_IS_TRUE(bottomRight.Succeeded());
        VERIFY_ARE_EQUAL(std::string{ "target-pane" }, bottomRight.root->first->paneId);
        VERIFY_ARE_EQUAL(std::string{ "slot-bottom-right" }, bottomRight.root->second->first->slotId);
        VERIFY_ARE_EQUAL(std::string{ "source-pane" }, bottomRight.root->second->second->paneId);
    }

    void WinTermDockingTests::RemovingPanePromotesSibling()
    {
        const auto root = LayoutNodeDescriptor::Split(
            SplitOrientation::Vertical,
            0.5,
            Pane("remove-pane"),
            Pane("keep-pane"));
        const auto result = LayoutTransformer::Remove(root, "remove-pane");
        VERIFY_IS_TRUE(result.Succeeded());
        VERIFY_ARE_EQUAL(std::string{ "remove-pane" }, result.removed->paneId);
        VERIFY_ARE_EQUAL(std::string{ "keep-pane" }, result.root->paneId);
    }

    void WinTermDockingTests::EmptySlotReplacementDoesNotAddSplit()
    {
        const auto result = LayoutTransformer::FillEmptySlot(
            LayoutNodeDescriptor::EmptySlot("slot-1"),
            "slot-1",
            Source());
        VERIFY_IS_TRUE(result.Succeeded());
        VERIFY_ARE_EQUAL(static_cast<int>(LayoutNodeType::Pane), static_cast<int>(result.root->type));
        VERIFY_ARE_EQUAL(std::string{ "source-pane" }, result.root->paneId);
    }

    void WinTermDockingTests::ValidatorRejectsDuplicatePaneAndSlotIds()
    {
        auto duplicatePane = LayoutNodeDescriptor::Split(
            SplitOrientation::Vertical,
            0.5,
            Pane("pane-1"),
            Pane("pane-1"));
        VERIFY_IS_FALSE(LayoutValidator::Validate(duplicatePane).IsValid());

        auto duplicateSlot = LayoutNodeDescriptor::Split(
            SplitOrientation::Horizontal,
            0.5,
            LayoutNodeDescriptor::EmptySlot("slot-1"),
            LayoutNodeDescriptor::EmptySlot("slot-1"));
        VERIFY_IS_FALSE(LayoutValidator::Validate(duplicateSlot).IsValid());
    }

    void WinTermDockingTests::GeometryUsesTransformedLayout()
    {
        const auto transformed = LayoutTransformer::Insert(Target(), std::nullopt, Source(), DockZone::Right, {});
        const auto geometry = LayoutGeometry::Calculate(transformed.root, { 0, 0, 1000, 600 });
        VERIFY_IS_TRUE(geometry.valid);
        VERIFY_ARE_EQUAL(size_t{ 3 }, geometry.entries.size());
        VERIFY_ARE_EQUAL(500.0, geometry.entries[1].bounds.width);
        VERIFY_ARE_EQUAL(500.0, geometry.entries[2].bounds.width);
        VERIFY_ARE_EQUAL(500.0, geometry.entries[2].bounds.x);
    }

    void WinTermDockingTests::WorkspaceV2RoundTripsEmptySlot()
    {
        const auto serialized = WorkspaceSerializer::Serialize(EmptyWorkspace());
        const auto restored = WorkspaceSerializer::Deserialize(serialized);
        VERIFY_ARE_EQUAL(uint32_t{ 2 }, restored.schemaVersion);
        VERIFY_ARE_EQUAL(uint32_t{ 1 }, restored.dockingModelVersion);
        VERIFY_ARE_EQUAL(std::string{ "slot-empty" }, restored.windows.front().tabs.front().layout->slotId);
        VERIFY_ARE_EQUAL(size_t{ 0 }, restored.PaneCount());
    }

    void WinTermDockingTests::WorkspaceV1MigrationIsIdempotent()
    {
        auto document = WorkspaceSerializer::ToJson(EmptyWorkspace());
        document["schemaVersion"] = 1;
        document.removeMember("dockingModelVersion");
        document["windows"][0]["tabs"][0]["panes"] = Json::Value{ Json::arrayValue };
        document["windows"][0]["tabs"][0]["layout"] = Json::Value{ Json::objectValue };
        document["windows"][0]["tabs"][0]["layout"]["type"] = "pane";
        document["windows"][0]["tabs"][0]["layout"]["paneId"] = "pane-v1";

        const auto migrated = WorkspaceMigration::Migrate(document);
        VERIFY_IS_TRUE(migrated.changed);
        VERIFY_ARE_EQUAL(uint32_t{ 2 }, migrated.targetVersion);
        VERIFY_ARE_EQUAL(std::string{ "pane" }, migrated.document["windows"][0]["tabs"][0]["layout"]["type"].asString());
        VERIFY_IS_TRUE(migrated.document["windows"][0]["tabs"][0]["layout"]["slotId"].isNull());

        const auto repeated = WorkspaceMigration::Migrate(migrated.document);
        VERIFY_IS_FALSE(repeated.changed);
        VERIFY_IS_TRUE(migrated.document == repeated.document);
    }
}
