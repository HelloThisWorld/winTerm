// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceRestoreService.h"

#include <algorithm>

using namespace winTerm::Workspaces;

namespace
{
    RestoreItemStatus CombinedStatus(const PaneRestorePlan& pane)
    {
        if (!pane.profile.profile)
        {
            return RestoreItemStatus::Failed;
        }
        if (pane.profile.status != ResolutionStatus::Exact || pane.directory.status != ResolutionStatus::Exact ||
            pane.theme.status != ResolutionStatus::Exact || pane.font.status != ResolutionStatus::Exact)
        {
            return RestoreItemStatus::RestoredWithFallback;
        }
        return RestoreItemStatus::Restored;
    }

    template<typename T, typename Predicate>
    void MovePreferredFirst(std::vector<T>& values, Predicate&& predicate)
    {
        const auto preferred = std::find_if(values.begin(), values.end(), std::forward<Predicate>(predicate));
        if (preferred != values.end() && preferred != values.begin())
        {
            std::rotate(values.begin(), preferred, std::next(preferred));
        }
    }
}

size_t WorkspaceRestoreReport::AdjustmentCount() const noexcept
{
    return std::count_if(items.begin(), items.end(), [](const auto& item) {
        return item.status == RestoreItemStatus::RestoredWithFallback;
    });
}

size_t WorkspaceRestoreReport::FailureCount() const noexcept
{
    return std::count_if(items.begin(), items.end(), [](const auto& item) {
        return item.status == RestoreItemStatus::Failed;
    });
}

WorkspaceRestorePlan WorkspaceRestoreService::Plan(WorkspaceDescriptor workspace, const WorkspaceRestoreContext& context)
{
    WorkspaceRestorePlan plan;
    plan.workspaceId = workspace.id;
    const auto validation = WorkspaceValidator::ValidateAndRepair(workspace);
    if (!validation.IsValid())
    {
        for (const auto& issue : validation.issues)
        {
            if (issue.severity == WorkspaceIssueSeverity::Error)
            {
                plan.report.items.push_back({ RestoreItemStatus::Failed, "workspace", workspace.id, issue.message });
            }
        }
        return plan;
    }
    for (const auto& issue : validation.issues)
    {
        plan.report.items.push_back({ RestoreItemStatus::RestoredWithFallback, "workspace", workspace.id, issue.message });
    }

    for (const auto& window : workspace.windows)
    {
        WindowRestorePlan windowPlan;
        windowPlan.windowId = window.id;
        windowPlan.activate = workspace.activeWindowId && *workspace.activeWindowId == window.id;
        windowPlan.monitor = MonitorResolver::Resolve(window, context.monitors, context.monitorOptions);
        if (windowPlan.monitor.status == ResolutionStatus::Unavailable)
        {
            plan.report.items.push_back({ RestoreItemStatus::Failed, "window", window.id, "No monitor is available for this window." });
            continue;
        }
        for (const auto& adjustment : windowPlan.monitor.adjustments)
        {
            plan.report.items.push_back({ RestoreItemStatus::RestoredWithFallback, "window", window.id, adjustment });
        }

        for (const auto& tab : window.tabs)
        {
            TabRestorePlan tabPlan;
            tabPlan.tabId = tab.id;
            tabPlan.activate = window.activeTabId && *window.activeTabId == tab.id;
            tabPlan.restoreZoomedPane = tab.zoomedPaneId.has_value();
            tabPlan.layout = tab.layout;
            for (const auto& pane : tab.panes)
            {
                PaneRestorePlan panePlan;
                panePlan.paneId = pane.id;
                panePlan.profile = ProfileResolver::Resolve(pane, context.profiles);
                const auto directoryContext = context.directoryContextForPane ? context.directoryContextForPane(pane, panePlan.profile) : context.defaultDirectoryContext;
                panePlan.directory = DirectoryResolver::Resolve(pane.workingDirectory, directoryContext);
                panePlan.theme = ThemeResolver::Resolve(pane.themeId, tab.themeId, context.profileTheme, context.globalTheme, context.themes);
                panePlan.font = FontResolver::Resolve(pane.font.family, context.profileFont, context.globalFont, context.fonts);
                panePlan.showUnexpectedEndNotice = pane.session.endedUnexpectedly;
                panePlan.showCommandWasRunningNotice = pane.session.wasCommandRunning;
                panePlan.status = CombinedStatus(panePlan);

                if (panePlan.status == RestoreItemStatus::Failed)
                {
                    plan.report.items.push_back({ RestoreItemStatus::Failed, "pane", pane.id, panePlan.profile.message });
                }
                else if (panePlan.status == RestoreItemStatus::RestoredWithFallback)
                {
                    std::string message;
                    for (const auto* candidate : { &panePlan.profile.message, &panePlan.directory.message, &panePlan.theme.message, &panePlan.font.message })
                    {
                        if (!candidate->empty())
                        {
                            if (!message.empty()) message += " ";
                            message += *candidate;
                        }
                    }
                    plan.report.items.push_back({ RestoreItemStatus::RestoredWithFallback, "pane", pane.id, std::move(message) });
                }
                tabPlan.panes.emplace_back(std::move(panePlan));
            }
            MovePreferredFirst(tabPlan.panes, [&](const auto& pane) {
                return tab.activePaneId && pane.paneId == *tab.activePaneId;
            });
            windowPlan.tabs.emplace_back(std::move(tabPlan));
        }
        MovePreferredFirst(windowPlan.tabs, [](const auto& tab) { return tab.activate; });
        plan.windows.emplace_back(std::move(windowPlan));
    }
    MovePreferredFirst(plan.windows, [](const auto& window) { return window.activate; });
    plan.canRestore = !plan.windows.empty();
    return plan;
}
