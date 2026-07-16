// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceValidator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

using namespace winTerm::Workspaces;

namespace
{
    struct ValidationContext
    {
        WorkspaceValidationResult result;
        std::unordered_set<std::string> ids;
        size_t totalPanes{};
        bool repair{};

        void Error(std::string code, std::string field, std::string message)
        {
            result.issues.push_back({ WorkspaceIssueSeverity::Error, std::move(code), std::move(field), std::move(message) });
        }

        void Warning(std::string code, std::string field, std::string message)
        {
            result.issues.push_back({ WorkspaceIssueSeverity::Warning, std::move(code), std::move(field), std::move(message) });
        }
    };

    bool IsFiniteRect(const Rect& value) noexcept
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.width) && std::isfinite(value.height);
    }

    bool HasControlCharacter(const std::string_view value) noexcept
    {
        return std::any_of(value.begin(), value.end(), [](const unsigned char character) {
            return character < 0x20 && character != '\t';
        });
    }

    void ValidateString(ValidationContext& context, const std::string_view value, const std::string& field, const bool required = false)
    {
        if (required && value.empty())
        {
            context.Error("required", field, "A required workspace string is empty.");
        }
        if (value.size() > MaximumWorkspaceStringLength)
        {
            context.Error("stringTooLong", field, "A workspace string exceeds the supported length.");
        }
        if (HasControlCharacter(value))
        {
            context.Error("controlCharacter", field, "A workspace string contains a control character.");
        }
    }

    void ValidateId(ValidationContext& context, const std::string& value, const std::string& field)
    {
        ValidateString(context, value, field, true);
        const auto safe = std::all_of(value.begin(), value.end(), [](const unsigned char character) {
            return std::isalnum(character) || character == '.' || character == '-' || character == '_';
        });
        if (!safe || value == "." || value == "..")
        {
            context.Error("unsafeId", field, "A workspace identifier cannot contain a path separator.");
        }
        if (!value.empty() && !context.ids.emplace(value).second)
        {
            context.Error("duplicateId", field, "Workspace identifiers must be globally unique.");
        }
    }

    void ValidateOptionalString(ValidationContext& context, const std::optional<std::string>& value, const std::string& field)
    {
        if (value)
        {
            ValidateString(context, *value, field);
        }
    }

    void ValidateLayout(
        ValidationContext& context,
        const std::shared_ptr<LayoutNodeDescriptor>& node,
        const std::unordered_set<std::string>& paneIds,
        std::unordered_set<std::string>& referencedPanes,
        std::unordered_set<const LayoutNodeDescriptor*>& ancestors,
        const std::string& field,
        const size_t depth)
    {
        if (!node)
        {
            context.Error("missingLayout", field, "A tab layout node is missing.");
            return;
        }
        if (depth > MaximumWorkspaceLayoutDepth)
        {
            context.Error("layoutTooDeep", field, "The pane layout exceeds the supported nesting depth.");
            return;
        }
        if (!ancestors.emplace(node.get()).second)
        {
            context.Error("circularLayout", field, "The pane layout contains a cycle.");
            return;
        }

        if (node->type == LayoutNodeType::Pane)
        {
            ValidateString(context, node->paneId, field + ".paneId", true);
            if (!paneIds.contains(node->paneId))
            {
                context.Error("missingPaneReference", field + ".paneId", "The layout references a pane that does not exist.");
            }
            else if (!referencedPanes.emplace(node->paneId).second)
            {
                context.Error("duplicatePaneReference", field + ".paneId", "The layout references a pane more than once.");
            }
            if (node->first || node->second)
            {
                context.Error("invalidPaneNode", field, "A pane layout node cannot have child nodes.");
            }
        }
        else
        {
            if (!std::isfinite(node->ratio) || node->ratio < MinimumSplitRatio || node->ratio > MaximumSplitRatio)
            {
                context.Warning("splitRatioAdjusted", field + ".ratio", "An invalid split ratio was replaced with the safe default.");
                if (context.repair)
                {
                    node->ratio = DefaultSplitRatio;
                    ++context.result.repairs;
                }
            }
            ValidateLayout(context, node->first, paneIds, referencedPanes, ancestors, field + ".first", depth + 1);
            ValidateLayout(context, node->second, paneIds, referencedPanes, ancestors, field + ".second", depth + 1);
            if (!node->paneId.empty())
            {
                context.Error("invalidSplitNode", field + ".paneId", "A split layout node cannot reference a pane directly.");
            }
        }
        ancestors.erase(node.get());
    }

    WorkspaceValidationResult ValidateWorkspace(WorkspaceDescriptor& workspace, const bool repair)
    {
        ValidationContext context;
        context.repair = repair;

        if (workspace.schemaVersion != WorkspaceSchemaVersion)
        {
            const auto newer = workspace.schemaVersion > WorkspaceSchemaVersion;
            context.Error(newer ? "newerSchema" : "migrationRequired", "schemaVersion",
                          newer ? "The workspace was created by a newer version of winTerm." : "The workspace requires schema migration.");
        }
        ValidateId(context, workspace.id, "id");
        ValidateString(context, workspace.name, "name", true);
        ValidateString(context, workspace.description, "description");
        ValidateString(context, workspace.createdAt, "createdAt", true);
        ValidateString(context, workspace.updatedAt, "updatedAt", true);
        ValidateString(context, workspace.applicationVersion, "applicationVersion", true);
        ValidateOptionalString(context, workspace.activeWindowId, "activeWindowId");
        ValidateOptionalString(context, workspace.lastOpenedAt, "lastOpenedAt");
        ValidateOptionalString(context, workspace.captureReason, "captureReason");
        for (size_t tagIndex = 0; tagIndex < workspace.tags.size(); ++tagIndex)
        {
            ValidateString(context, workspace.tags[tagIndex], "tags[" + std::to_string(tagIndex) + "]");
        }

        if (workspace.windows.empty())
        {
            context.Error("emptyWorkspace", "windows", "A workspace must contain at least one window.");
        }
        if (workspace.windows.size() > MaximumWorkspaceWindows)
        {
            context.Error("tooManyWindows", "windows", "The workspace contains too many windows.");
        }

        std::unordered_set<std::string> windowIds;
        for (size_t windowIndex = 0; windowIndex < workspace.windows.size(); ++windowIndex)
        {
            auto& window = workspace.windows[windowIndex];
            const auto windowField = "windows[" + std::to_string(windowIndex) + "]";
            ValidateId(context, window.id, windowField + ".id");
            windowIds.emplace(window.id);
            ValidateString(context, window.displayName, windowField + ".displayName");
            ValidateString(context, window.monitor.deviceId, windowField + ".monitor.deviceId");
            ValidateString(context, window.monitor.stableId, windowField + ".monitor.stableId");
            ValidateString(context, window.monitor.friendlyName, windowField + ".monitor.friendlyName");
            ValidateOptionalString(context, window.activeTabId, windowField + ".activeTabId");

            if (!IsFiniteRect(window.bounds) || window.bounds.width <= 0 || window.bounds.height <= 0)
            {
                context.Error("invalidBounds", windowField + ".bounds", "Window bounds must contain finite positive dimensions.");
            }
            if (!IsFiniteRect(window.normalizedBounds) || window.normalizedBounds.width <= 0 || window.normalizedBounds.height <= 0)
            {
                context.Error("invalidNormalizedBounds", windowField + ".normalizedBounds", "Normalized window bounds must contain finite positive dimensions.");
            }
            if (!IsFiniteRect(window.monitor.workArea) || window.monitor.workArea.width <= 0 || window.monitor.workArea.height <= 0)
            {
                context.Error("invalidWorkArea", windowField + ".monitor.workArea", "Monitor work area must contain finite positive dimensions.");
            }
            if (window.monitor.dpiX < 48 || window.monitor.dpiX > 960 || window.monitor.dpiY < 48 || window.monitor.dpiY > 960)
            {
                context.Error("invalidDpi", windowField + ".monitor", "Monitor DPI is outside the supported range.");
            }
            if (window.tabs.empty())
            {
                context.Error("emptyWindow", windowField + ".tabs", "A restored window must contain at least one tab.");
            }
            if (window.tabs.size() > MaximumWorkspaceTabsPerWindow)
            {
                context.Error("tooManyTabs", windowField + ".tabs", "A window contains too many tabs.");
            }

            std::unordered_set<std::string> tabIds;
            for (size_t tabIndex = 0; tabIndex < window.tabs.size(); ++tabIndex)
            {
                auto& tab = window.tabs[tabIndex];
                const auto tabField = windowField + ".tabs[" + std::to_string(tabIndex) + "]";
                ValidateId(context, tab.id, tabField + ".id");
                tabIds.emplace(tab.id);
                ValidateString(context, tab.title, tabField + ".title");
                ValidateOptionalString(context, tab.themeId, tabField + ".themeId");
                ValidateOptionalString(context, tab.activePaneId, tabField + ".activePaneId");
                ValidateOptionalString(context, tab.zoomedPaneId, tabField + ".zoomedPaneId");
                ValidateOptionalString(context, tab.tabColor, tabField + ".tabColor");

                if (tab.panes.empty())
                {
                    context.Error("emptyTab", tabField + ".panes", "A restored tab must contain at least one pane.");
                }
                if (tab.panes.size() > MaximumWorkspacePanesPerTab)
                {
                    context.Error("tooManyPanes", tabField + ".panes", "A tab contains too many panes.");
                }
                context.totalPanes += tab.panes.size();

                std::unordered_set<std::string> paneIds;
                for (size_t paneIndex = 0; paneIndex < tab.panes.size(); ++paneIndex)
                {
                    const auto& pane = tab.panes[paneIndex];
                    const auto paneField = tabField + ".panes[" + std::to_string(paneIndex) + "]";
                    ValidateId(context, pane.id, paneField + ".id");
                    paneIds.emplace(pane.id);
                    ValidateString(context, pane.profileId, paneField + ".profileId");
                    ValidateString(context, pane.profileNameFallback, paneField + ".profileNameFallback");
                    ValidateString(context, pane.profileSource, paneField + ".profileSource");
                    ValidateString(context, pane.stableProfileId, paneField + ".stableProfileId");
                    ValidateString(context, pane.workingDirectory.value, paneField + ".workingDirectory.value");
                    ValidateString(context, pane.workingDirectory.verifiedAt, paneField + ".workingDirectory.verifiedAt");
                    ValidateOptionalString(context, pane.workingDirectory.distributionId, paneField + ".workingDirectory.distributionId");
                    ValidateString(context, pane.startingDirectoryFallback, paneField + ".startingDirectoryFallback");
                    ValidateString(context, pane.title, paneField + ".title");
                    ValidateOptionalString(context, pane.themeId, paneField + ".themeId");
                    ValidateOptionalString(context, pane.font.family, paneField + ".font.family");
                    if (pane.font.size && (!std::isfinite(*pane.font.size) || *pane.font.size < 1 || *pane.font.size > 512))
                    {
                        context.Error("invalidFontSize", paneField + ".font.size", "Font size is outside the supported range.");
                    }
                    if (pane.font.weight && (*pane.font.weight < 1 || *pane.font.weight > 1000))
                    {
                        context.Error("invalidFontWeight", paneField + ".font.weight", "Font weight is outside the supported range.");
                    }
                    if (pane.workingDirectory.kind == WorkingDirectoryKind::Wsl && !pane.workingDirectory.value.empty() && pane.workingDirectory.value.front() != '/')
                    {
                        context.Error("invalidWslPath", paneField + ".workingDirectory.value", "A WSL working directory must use the WSL path namespace.");
                    }
                    if (pane.workingDirectory.kind == WorkingDirectoryKind::Remote && pane.workingDirectory.source == WorkingDirectorySource::Launch)
                    {
                        context.Warning("remoteDirectoryNotRestored", paneField + ".workingDirectory", "Remote working directories are metadata only and are not auto-connected.");
                    }
                }

                if (tab.activePaneId && !paneIds.contains(*tab.activePaneId))
                {
                    if (repair && !tab.panes.empty())
                    {
                        tab.activePaneId = tab.panes.front().id;
                        ++context.result.repairs;
                        context.Warning("activePaneRepaired", tabField + ".activePaneId", "The missing active pane was replaced with the first pane.");
                    }
                    else
                    {
                        context.Error("invalidActivePane", tabField + ".activePaneId", "The active pane does not exist in the tab.");
                    }
                }
                if (tab.zoomedPaneId && !paneIds.contains(*tab.zoomedPaneId))
                {
                    if (repair)
                    {
                        tab.zoomedPaneId.reset();
                        ++context.result.repairs;
                        context.Warning("zoomedPaneCleared", tabField + ".zoomedPaneId", "The missing zoomed pane reference was cleared.");
                    }
                    else
                    {
                        context.Error("invalidZoomedPane", tabField + ".zoomedPaneId", "The zoomed pane does not exist in the tab.");
                    }
                }

                std::unordered_set<std::string> referencedPanes;
                std::unordered_set<const LayoutNodeDescriptor*> ancestors;
                ValidateLayout(context, tab.layout, paneIds, referencedPanes, ancestors, tabField + ".layout", 1);
                if (referencedPanes.size() != paneIds.size())
                {
                    context.Error("unreferencedPane", tabField + ".layout", "Every pane must appear exactly once in the layout tree.");
                }
            }

            if (window.activeTabId && !tabIds.contains(*window.activeTabId))
            {
                if (repair && !window.tabs.empty())
                {
                    window.activeTabId = window.tabs.front().id;
                    ++context.result.repairs;
                    context.Warning("activeTabRepaired", windowField + ".activeTabId", "The missing active tab was replaced with the first tab.");
                }
                else
                {
                    context.Error("invalidActiveTab", windowField + ".activeTabId", "The active tab does not exist in the window.");
                }
            }
        }

        if (context.totalPanes > MaximumWorkspacePanes)
        {
            context.Error("tooManyTotalPanes", "windows", "The workspace contains too many panes.");
        }
        if (workspace.activeWindowId && !windowIds.contains(*workspace.activeWindowId))
        {
            if (repair && !workspace.windows.empty())
            {
                workspace.activeWindowId = workspace.windows.front().id;
                ++context.result.repairs;
                context.Warning("activeWindowRepaired", "activeWindowId", "The missing active window was replaced with the first window.");
            }
            else
            {
                context.Error("invalidActiveWindow", "activeWindowId", "The active window does not exist in the workspace.");
            }
        }
        return context.result;
    }
}

bool WorkspaceValidationResult::IsValid() const noexcept
{
    return std::none_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == WorkspaceIssueSeverity::Error;
    });
}

bool WorkspaceValidationResult::HasWarnings() const noexcept
{
    return std::any_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == WorkspaceIssueSeverity::Warning;
    });
}

std::vector<std::string> WorkspaceValidationResult::ErrorMessages() const
{
    std::vector<std::string> result;
    for (const auto& issue : issues)
    {
        if (issue.severity == WorkspaceIssueSeverity::Error)
        {
            result.emplace_back(issue.message);
        }
    }
    return result;
}

WorkspaceValidationResult WorkspaceValidator::Validate(const WorkspaceDescriptor& workspace)
{
    auto copy = workspace;
    return ValidateWorkspace(copy, false);
}

WorkspaceValidationResult WorkspaceValidator::ValidateAndRepair(WorkspaceDescriptor& workspace)
{
    return ValidateWorkspace(workspace, true);
}
