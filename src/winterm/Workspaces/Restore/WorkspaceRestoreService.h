// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "WorkspaceResolvers.h"
#include "../Persistence/WorkspaceValidator.h"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace winTerm::Workspaces
{
    enum class RestoreItemStatus
    {
        Restored,
        RestoredWithFallback,
        Skipped,
        Failed,
    };

    struct WorkspaceRestoreReportItem
    {
        RestoreItemStatus status{ RestoreItemStatus::Restored };
        std::string itemType;
        std::string itemId;
        std::string message;
    };

    struct WorkspaceRestoreReport
    {
        std::vector<WorkspaceRestoreReportItem> items;

        size_t AdjustmentCount() const noexcept;
        size_t FailureCount() const noexcept;
    };

    struct PaneRestorePlan
    {
        std::string paneId;
        ProfileResolution profile;
        DirectoryResolution directory;
        StringResourceResolution theme;
        StringResourceResolution font;
        bool showUnexpectedEndNotice{ false };
        bool showCommandWasRunningNotice{ false };
        RestoreItemStatus status{ RestoreItemStatus::Restored };
    };

    struct TabRestorePlan
    {
        std::string tabId;
        bool activate{ false };
        bool restoreZoomedPane{ false };
        std::shared_ptr<LayoutNodeDescriptor> layout;
        std::vector<PaneRestorePlan> panes;
    };

    struct WindowRestorePlan
    {
        std::string windowId;
        bool activate{ false };
        MonitorResolution monitor;
        std::vector<TabRestorePlan> tabs;
    };

    struct WorkspaceRestorePlan
    {
        std::string workspaceId;
        std::vector<WindowRestorePlan> windows;
        WorkspaceRestoreReport report;
        bool canRestore{ false };
    };

    struct WorkspaceRestoreContext
    {
        std::vector<ProfileCandidate> profiles;
        std::vector<MonitorDescriptor> monitors;
        std::set<std::string, std::less<>> themes;
        std::set<std::string, std::less<>> fonts;
        std::string profileTheme;
        std::string globalTheme{ "winterm.midnight" };
        std::string profileFont;
        std::string globalFont;
        DirectoryResolutionContext defaultDirectoryContext;
        std::function<DirectoryResolutionContext(const PaneDescriptor&, const ProfileResolution&)> directoryContextForPane;
        MonitorResolutionOptions monitorOptions;
    };

    class WorkspaceRestoreService final
    {
    public:
        static WorkspaceRestorePlan Plan(WorkspaceDescriptor workspace, const WorkspaceRestoreContext& context);
    };
}
