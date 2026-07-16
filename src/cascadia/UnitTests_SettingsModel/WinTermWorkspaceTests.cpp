// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../../winterm/Workspaces/Capture/WorkspaceCaptureService.h"
#include "../../winterm/Workspaces/ImportExport/WorkspaceImportExport.h"
#include "../../winterm/Workspaces/Persistence/WorkspaceSerializer.h"
#include "../../winterm/Workspaces/Persistence/WorkspaceStore.h"
#include "../../winterm/Workspaces/Persistence/WorkspaceValidator.h"
#include "../../winterm/Workspaces/Persistence/WorkspaceMigration.h"
#include "../../winterm/Workspaces/Restore/WorkspaceRestoreService.h"
#include "../../winterm/Workspaces/Runtime/WorkspaceRuntime.h"

#include <chrono>
#include <limits>

using namespace WEX::TestExecution;
using namespace winTerm::Workspaces;

namespace
{
    WorkspaceDescriptor ValidWorkspace(const size_t paneCount = 1)
    {
        WorkspaceDescriptor workspace;
        workspace.id = "workspace.test";
        workspace.name = "Test Workspace";
        workspace.description = "Workspace fixture";
        workspace.createdAt = "2026-07-15T00:00:00Z";
        workspace.updatedAt = "2026-07-15T00:00:00Z";
        workspace.activeWindowId = "window-1";

        WindowDescriptor window;
        window.id = "window-1";
        window.displayName = "Test";
        window.monitor.deviceId = "DISPLAY-A";
        window.monitor.stableId = "monitor-a";
        window.monitor.friendlyName = "Primary";
        window.monitor.workArea = { 0, 0, 1920, 1080 };
        window.monitor.isPrimary = true;
        window.bounds = { 100, 80, 1200, 800 };
        window.normalizedBounds = { 0.05, 0.07, 0.625, 0.74 };
        window.activeTabId = "tab-1";

        TabDescriptor tab;
        tab.id = "tab-1";
        tab.title = "編譯";
        tab.customTitle = true;
        tab.activePaneId = "pane-1";
        for (size_t index = 0; index < paneCount; ++index)
        {
            PaneDescriptor pane;
            pane.id = "pane-" + std::to_string(index + 1);
            pane.profileId = "{profile-powershell}";
            pane.profileNameFallback = "PowerShell";
            pane.profileSource = "Windows.Terminal.PowershellCore";
            pane.stableProfileId = "powershell";
            pane.shellType = ShellType::PowerShell;
            pane.workingDirectory = { WorkingDirectoryKind::Windows, "D:\\Code\\中文", WorkingDirectorySource::ShellIntegration, "2026-07-15T00:00:00Z", std::nullopt };
            pane.startingDirectoryFallback = "D:\\Code";
            pane.title = "PowerShell";
            tab.panes.emplace_back(std::move(pane));
        }
        if (paneCount == 1)
        {
            tab.layout = LayoutNodeDescriptor::Pane("pane-1");
        }
        else
        {
            tab.layout = LayoutNodeDescriptor::Split(
                SplitOrientation::Vertical,
                0.58,
                LayoutNodeDescriptor::Pane("pane-1"),
                LayoutNodeDescriptor::Pane("pane-2"));
        }
        window.tabs.emplace_back(std::move(tab));
        workspace.windows.emplace_back(std::move(window));
        return workspace;
    }

    std::filesystem::path TestDirectory()
    {
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        return std::filesystem::temp_directory_path() / ("winterm-workspace-tests-" + std::to_string(tick));
    }
}

namespace SettingsModelUnitTests
{
    class WinTermWorkspaceTests
    {
        TEST_CLASS(WinTermWorkspaceTests);

        TEST_METHOD(ModelValidationRejectsUnsafeGraphs);
        TEST_METHOD(SerializationIsDeterministicAndPreservesUnicode);
        TEST_METHOD(InvalidSplitRatioIsRepaired);
        TEST_METHOD(MigrationIsDeterministicAndIdempotent);
        TEST_METHOD(ImportRejectsExecutableContent);
        TEST_METHOD(MonitorAndDirectoryResolversFallBackSafely);
        TEST_METHOD(OlderGenerationCannotOverwriteNewerState);
        TEST_METHOD(StoreRetainsLastValidBackup);
        TEST_METHOD(RestorePlanningIsolatesPaneFailures);
    };

    void WinTermWorkspaceTests::ModelValidationRejectsUnsafeGraphs()
    {
        auto workspace = ValidWorkspace();
        VERIFY_IS_TRUE(WorkspaceValidator::Validate(workspace).IsValid());

        workspace.windows.front().tabs.front().panes.front().id = workspace.windows.front().id;
        VERIFY_IS_FALSE(WorkspaceValidator::Validate(workspace).IsValid());

        workspace = ValidWorkspace(2);
        workspace.windows.front().tabs.front().layout->first = workspace.windows.front().tabs.front().layout;
        VERIFY_IS_FALSE(WorkspaceValidator::Validate(workspace).IsValid());
    }

    void WinTermWorkspaceTests::SerializationIsDeterministicAndPreservesUnicode()
    {
        const auto workspace = ValidWorkspace();
        const auto first = WorkspaceSerializer::Serialize(workspace);
        const auto second = WorkspaceSerializer::Serialize(workspace);
        VERIFY_ARE_EQUAL(first, second);

        const auto restored = WorkspaceSerializer::Deserialize(first);
        VERIFY_ARE_EQUAL(std::string{ "編譯" }, restored.windows.front().tabs.front().title);
        VERIFY_ARE_EQUAL(std::string{ "D:\\Code\\中文" }, restored.windows.front().tabs.front().panes.front().workingDirectory.value);

        auto invalid = workspace;
        invalid.windows.front().bounds.width = std::numeric_limits<double>::infinity();
        VERIFY_THROWS(WorkspaceSerializer::Serialize(invalid), std::runtime_error);
    }

    void WinTermWorkspaceTests::InvalidSplitRatioIsRepaired()
    {
        auto workspace = ValidWorkspace(2);
        workspace.windows.front().tabs.front().layout->ratio = -1;
        const auto validation = WorkspaceValidator::ValidateAndRepair(workspace);
        VERIFY_IS_TRUE(validation.IsValid());
        VERIFY_IS_TRUE(validation.HasWarnings());
        VERIFY_ARE_EQUAL(DefaultSplitRatio, workspace.windows.front().tabs.front().layout->ratio);
    }

    void WinTermWorkspaceTests::MigrationIsDeterministicAndIdempotent()
    {
        Json::Value legacy{ Json::objectValue };
        legacy["schemaVersion"] = 0;
        legacy["workspaceId"] = "workspace.legacy";
        legacy["name"] = "Legacy";
        legacy["createdAt"] = "2026-07-15T00:00:00Z";
        legacy["updatedAt"] = "2026-07-15T00:00:00Z";
        legacy["windows"] = Json::Value{ Json::arrayValue };

        const auto migrated = WorkspaceMigration::Migrate(legacy);
        VERIFY_IS_TRUE(migrated.changed);
        VERIFY_ARE_EQUAL(uint32_t{ 1 }, migrated.targetVersion);
        VERIFY_ARE_EQUAL(std::string{ "workspace.legacy" }, migrated.document["id"].asString());

        const auto repeated = WorkspaceMigration::Migrate(migrated.document);
        VERIFY_IS_FALSE(repeated.changed);
        VERIFY_IS_TRUE(migrated.document == repeated.document);
    }

    void WinTermWorkspaceTests::ImportRejectsExecutableContent()
    {
        auto json = WorkspaceSerializer::ToJson(ValidWorkspace());
        json["command"] = "Remove-Item -Recurse -Force .";
        Json::StreamWriterBuilder builder;
        const auto rejectedCommand = WorkspaceImporter::Inspect(Json::writeString(builder, json));
        VERIFY_ARE_EQUAL(WorkspaceImportStatus::Rejected, rejectedCommand.status);

        json.removeMember("command");
        json["windows"][0]["tabs"][0]["panes"][0]["executablePath"] = "C:\\Temp\\untrusted.exe";
        const auto rejectedExecutable = WorkspaceImporter::Inspect(Json::writeString(builder, json));
        VERIFY_ARE_EQUAL(WorkspaceImportStatus::Rejected, rejectedExecutable.status);

        json.removeMember("windows");
        json = WorkspaceSerializer::ToJson(ValidWorkspace());
        json["schemaVersion"] = WorkspaceSchemaVersion + 1;
        const auto newer = WorkspaceImporter::Inspect(Json::writeString(builder, json));
        VERIFY_ARE_EQUAL(WorkspaceImportStatus::NewerSchemaPreserved, newer.status);

        json = WorkspaceSerializer::ToJson(ValidWorkspace());
        json["safeNote"] = "ignored extension metadata";
        const auto safeUnknown = WorkspaceImporter::Inspect(Json::writeString(builder, json));
        VERIFY_ARE_EQUAL(WorkspaceImportStatus::ReadyForConfirmation, safeUnknown.status);
        VERIFY_ARE_EQUAL(size_t{ 1 }, safeUnknown.summary.ignoredFields.size());
    }

    void WinTermWorkspaceTests::MonitorAndDirectoryResolversFallBackSafely()
    {
        auto workspace = ValidWorkspace();
        workspace.windows.front().monitor.deviceId = "REMOVED";
        workspace.windows.front().monitor.stableId = "REMOVED";
        workspace.windows.front().monitor.friendlyName = "Removed monitor";
        MonitorDescriptor primary{ "PRIMARY", "primary", "Primary", { 0, 0, 1280, 720 }, 144, 144, true };
        const auto monitor = MonitorResolver::Resolve(workspace.windows.front(), { primary });
        VERIFY_ARE_EQUAL(std::string{ "PRIMARY" }, monitor.monitor.deviceId);
        VERIFY_IS_TRUE(monitor.bounds.width <= primary.workArea.width);
        VERIFY_IS_TRUE(monitor.bounds.height <= primary.workArea.height);

        WorkingDirectoryDescriptor missing{ WorkingDirectoryKind::Windows, "D:\\Missing\\Child", WorkingDirectorySource::ShellIntegration };
        DirectoryResolutionContext context;
        context.profileStartingDirectory = "D:\\Profile";
        context.userHomeDirectory = "C:\\Users\\test";
        context.pathExists = [](const std::string_view path) { return path == "D:\\Missing"; };
        const auto directory = DirectoryResolver::Resolve(missing, context);
        VERIFY_ARE_EQUAL(ResolutionStatus::MatchedFallback, directory.status);
        VERIFY_ARE_EQUAL(std::string{ "D:\\Missing" }, directory.value);

        missing.kind = WorkingDirectoryKind::Unc;
        missing.value = "\\\\server\\share";
        const auto unc = DirectoryResolver::Resolve(missing, context);
        VERIFY_ARE_EQUAL(ResolutionStatus::Deferred, unc.status);
    }

    void WinTermWorkspaceTests::OlderGenerationCannotOverwriteNewerState()
    {
        WorkspaceDirtyTracker tracker;
        const auto generation1 = tracker.MarkDirty();
        const auto generation2 = tracker.MarkDirty();
        VERIFY_IS_FALSE(tracker.TryCommit(generation1));
        VERIFY_IS_TRUE(tracker.TryCommit(generation2));
        VERIFY_IS_FALSE(tracker.IsDirty());
    }

    void WinTermWorkspaceTests::StoreRetainsLastValidBackup()
    {
        const auto directory = TestDirectory();
        try
        {
            WorkspaceStore store{ directory };
            auto first = ValidWorkspace();
            first.name = "First";
            VERIFY_IS_TRUE(store.SaveLastSession(first, 1).succeeded);
            auto second = first;
            second.name = "Second";
            second.updatedAt = "2026-07-15T00:01:00Z";
            VERIFY_IS_TRUE(store.SaveLastSession(second, 2).succeeded);

            const auto backup = WorkspaceSerializer::Deserialize(WorkspaceSerializer::ReadFile(store.Paths().lastSessionBackup));
            VERIFY_ARE_EQUAL(std::string{ "First" }, backup.name);
            VERIFY_IS_FALSE(store.SaveLastSession(first, 1).succeeded);
        }
        catch (...)
        {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            throw;
        }
        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }

    void WinTermWorkspaceTests::RestorePlanningIsolatesPaneFailures()
    {
        auto workspace = ValidWorkspace(2);
        workspace.windows.front().tabs.front().panes[1].profileId = "missing";
        workspace.windows.front().tabs.front().panes[1].profileSource.clear();
        workspace.windows.front().tabs.front().panes[1].stableProfileId.clear();
        workspace.windows.front().tabs.front().panes[1].profileNameFallback = "Missing";
        workspace.windows.front().tabs.front().panes[1].shellType = ShellType::Wsl;

        WorkspaceRestoreContext context;
        context.profiles.push_back({ "{profile-powershell}", "Windows.Terminal.PowershellCore", "powershell", "PowerShell", ShellType::PowerShell, false, true });
        context.monitors.push_back(workspace.windows.front().monitor);
        context.themes.emplace("winterm.midnight");
        context.fonts.emplace("Cascadia Mono NF");
        context.defaultDirectoryContext.userHomeDirectory = "C:\\Users\\test";
        context.defaultDirectoryContext.pathExists = [](const std::string_view) { return true; };

        const auto plan = WorkspaceRestoreService::Plan(workspace, context);
        VERIFY_IS_TRUE(plan.canRestore);
        VERIFY_ARE_EQUAL(size_t{ 2 }, plan.windows.front().tabs.front().panes.size());
        VERIFY_ARE_EQUAL(size_t{ 1 }, plan.report.FailureCount());
    }
}
