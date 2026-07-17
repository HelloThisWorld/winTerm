// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceImportExport.h"

#include "../Persistence/WorkspaceSerializer.h"
#include "../Persistence/WorkspaceMigration.h"
#include "../Persistence/WorkspaceValidator.h"

#include <algorithm>
#include <cctype>
#include <set>

using namespace winTerm::Workspaces;

namespace
{
    std::string Lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    bool IsForbiddenKey(const std::string_view key)
    {
        static const std::set<std::string, std::less<>> forbidden{
            "autorun",
            "command",
            "commandline",
            "credential",
            "credentials",
            "environment",
            "environmentvariables",
            "executable",
            "executablepath",
            "externalfile",
            "include",
            "includes",
            "plugin",
            "plugins",
            "privatekey",
            "privatekeypath",
            "registry",
            "script",
            "shellarguments",
            "sshprivatekey",
            "startupcommand",
            "token",
            "password",
            "url",
        };
        return forbidden.contains(key);
    }

    bool IsExternalReference(const std::string_view value)
    {
        const auto lower = Lower(std::string{ value.substr(0, std::min<size_t>(value.size(), 16)) });
        return lower.starts_with("http://") || lower.starts_with("https://") || lower.starts_with("file://");
    }

    bool IsKnownWorkspaceField(const std::string_view field)
    {
        static const std::set<std::string, std::less<>> fields{
            "schemaVersion", "id", "name", "description", "createdAt", "updatedAt", "source",
            "applicationVersion", "protocolVersion", "dockingModelVersion", "startupBehavior", "activeWindowId", "tags",
            "isDefault", "lastOpenedAt", "captureReason", "recoveryMetadata", "layoutHistoryMetadata", "windows",
            "restoreFocus", "restoreWindowState", "generation", "cleanShutdown", "capturedAt",
            "limit", "lastSequence",
            "displayName", "monitor", "bounds", "normalizedBounds", "windowState", "isAlwaysOnTop",
            "activeTabId", "tabs", "deviceId", "stableId", "friendlyName", "workArea", "dpiX",
            "dpiY", "isPrimary", "x", "y", "width", "height", "title", "customTitle", "themeId",
            "activePaneId", "zoomedPaneId", "tabColor", "isReadOnly", "panes", "layout", "profileId",
            "profileNameFallback", "profileSource", "stableProfileId", "shellType", "workingDirectory",
            "startingDirectoryFallback", "font", "session", "kind", "value", "verifiedAt",
            "distributionId", "family", "size", "weight", "lastExitCode", "wasCommandRunning",
            "endedUnexpectedly", "type", "paneId", "slotId", "preferredProfileId", "orientation", "ratio", "first", "second",
        };
        return fields.contains(field);
    }

    void CollectIgnoredFields(const Json::Value& value, const std::string& path, std::vector<std::string>& fields)
    {
        if (value.isObject())
        {
            for (const auto& name : value.getMemberNames())
            {
                if (!IsKnownWorkspaceField(name))
                {
                    fields.emplace_back(path + "." + name);
                }
                CollectIgnoredFields(value[name], path + "." + name, fields);
            }
        }
        else if (value.isArray())
        {
            for (Json::ArrayIndex index = 0; index < value.size(); ++index)
            {
                CollectIgnoredFields(value[index], path + "[" + std::to_string(index) + "]", fields);
            }
        }
    }

    void InspectNode(const Json::Value& value, const std::string& path, const size_t depth, std::vector<std::string>& issues)
    {
        if (depth > MaximumWorkspaceLayoutDepth + 16)
        {
            issues.emplace_back("The imported workspace exceeds the maximum nesting depth.");
            return;
        }
        if (value.isObject())
        {
            for (const auto& name : value.getMemberNames())
            {
                const auto lowerName = Lower(name);
                if (IsForbiddenKey(lowerName))
                {
                    issues.emplace_back("Forbidden field: " + path + "." + name);
                }
                InspectNode(value[name], path + "." + name, depth + 1, issues);
            }
        }
        else if (value.isArray())
        {
            for (Json::ArrayIndex index = 0; index < value.size(); ++index)
            {
                InspectNode(value[index], path + "[" + std::to_string(index) + "]", depth + 1, issues);
            }
        }
        else if (value.isString() && IsExternalReference(value.asString()))
        {
            issues.emplace_back("External URL or file reference: " + path);
        }
    }

    void AddUnique(std::vector<std::string>& values, const std::string& value)
    {
        if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end())
        {
            values.emplace_back(value);
        }
    }

    WorkspaceImportSummary Summarize(const WorkspaceDescriptor& workspace)
    {
        WorkspaceImportSummary summary;
        summary.name = workspace.name;
        summary.windowCount = workspace.windows.size();
        summary.tabCount = workspace.TabCount();
        summary.paneCount = workspace.PaneCount();
        for (const auto& window : workspace.windows)
        {
            for (const auto& tab : window.tabs)
            {
                if (tab.themeId) AddUnique(summary.referencedThemes, *tab.themeId);
                for (const auto& pane : tab.panes)
                {
                    AddUnique(summary.referencedProfiles, pane.profileId.empty() ? pane.profileNameFallback : pane.profileId);
                    if (pane.themeId) AddUnique(summary.referencedThemes, *pane.themeId);
                    if (pane.font.family) AddUnique(summary.referencedFonts, *pane.font.family);
                }
            }
        }
        return summary;
    }

    std::string RedactPath(std::string value, const WorkspaceExportOptions& options)
    {
        if (!options.redactUserHome || options.userHomeDirectory.empty() || value.size() < options.userHomeDirectory.size())
        {
            return value;
        }
        const auto prefixMatches = std::equal(options.userHomeDirectory.begin(), options.userHomeDirectory.end(), value.begin(), [](const char left, const char right) {
            return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
        });
        if (prefixMatches && (value.size() == options.userHomeDirectory.size() || value[options.userHomeDirectory.size()] == '\\' || value[options.userHomeDirectory.size()] == '/'))
        {
            value.replace(0, options.userHomeDirectory.size(), "<USER_HOME>");
        }
        return value;
    }
}

std::vector<std::string> WorkspaceImportSanitizer::Inspect(const Json::Value& document)
{
    std::vector<std::string> issues;
    InspectNode(document, "$", 0, issues);
    return issues;
}

WorkspaceDescriptor WorkspaceImportSanitizer::Sanitize(WorkspaceDescriptor workspace)
{
    workspace.source = WorkspaceSource::Imported;
    workspace.isDefault = false;
    workspace.lastOpenedAt.reset();
    workspace.captureReason = "import";
    workspace.recoveryMetadata.reset();
    for (auto& window : workspace.windows)
    {
        window.monitor.isPrimary = false;
        for (auto& tab : window.tabs)
        {
            for (auto& pane : tab.panes)
            {
                pane.session.wasCommandRunning = false;
                pane.session.endedUnexpectedly = false;
                pane.session.lastExitCode.reset();
                if (pane.workingDirectory.kind == WorkingDirectoryKind::Remote)
                {
                    pane.workingDirectory.value.clear();
                    pane.workingDirectory.source = WorkingDirectorySource::Unknown;
                }
            }
        }
    }
    return workspace;
}

WorkspaceImportResult WorkspaceImporter::InspectFile(const std::filesystem::path& path)
{
    try
    {
        return Inspect(WorkspaceSerializer::ReadFile(path));
    }
    catch (const std::exception& exception)
    {
        return { WorkspaceImportStatus::Rejected, std::nullopt, {}, exception.what() };
    }
}

WorkspaceImportResult WorkspaceImporter::Inspect(const std::string_view content)
{
    try
    {
        auto document = WorkspaceSerializer::ParseJsonDocument(content);
        if (!document["schemaVersion"].isUInt())
        {
            return { WorkspaceImportStatus::Rejected, std::nullopt, {}, "The workspace schema version is missing or invalid." };
        }
        if (document["schemaVersion"].asUInt() > WorkspaceSchemaVersion)
        {
            WorkspaceImportResult result;
            result.status = WorkspaceImportStatus::NewerSchemaPreserved;
            result.message = "This workspace was created by a newer version of winTerm. The source file was not modified or loaded.";
            return result;
        }
        if (document["schemaVersion"].asUInt() < WorkspaceSchemaVersion)
        {
            document = WorkspaceMigration::Migrate(document).document;
        }
        const auto securityIssues = WorkspaceImportSanitizer::Inspect(document);
        if (!securityIssues.empty())
        {
            WorkspaceImportResult result;
            result.status = WorkspaceImportStatus::Rejected;
            result.summary.securityWarnings = securityIssues;
            result.message = "The workspace contains fields or references that imports are not allowed to use.";
            return result;
        }
        auto workspace = WorkspaceSerializer::FromJson(document);
        workspace = WorkspaceImportSanitizer::Sanitize(std::move(workspace));
        auto summary = Summarize(workspace);
        CollectIgnoredFields(document, "$", summary.ignoredFields);
        return { WorkspaceImportStatus::ReadyForConfirmation, std::move(workspace), std::move(summary), "The workspace is ready for user confirmation." };
    }
    catch (const std::exception& exception)
    {
        return { WorkspaceImportStatus::Rejected, std::nullopt, {}, exception.what() };
    }
}

WorkspaceDescriptor WorkspaceExporter::Prepare(const WorkspaceDescriptor& workspace, const WorkspaceExportOptions& options)
{
    auto result = workspace;
    result.isDefault = false;
    result.lastOpenedAt.reset();
    result.captureReason = "export";
    result.recoveryMetadata.reset();
    if (!options.includeDescriptionAndTags)
    {
        result.description.clear();
        result.tags.clear();
    }
    for (auto& window : result.windows)
    {
        if (!options.includeMonitorMetadata)
        {
            window.monitor.deviceId.clear();
            window.monitor.stableId.clear();
            window.monitor.friendlyName.clear();
            window.monitor.isPrimary = false;
        }
        for (auto& tab : window.tabs)
        {
            if (!options.includeCustomTitles && tab.customTitle)
            {
                tab.title.clear();
                tab.customTitle = false;
            }
            for (auto& pane : tab.panes)
            {
                if (!options.includeCustomTitles)
                {
                    pane.title.clear();
                }
                if (!options.includeWorkingDirectories)
                {
                    pane.workingDirectory = {};
                    pane.startingDirectoryFallback.clear();
                }
                else
                {
                    pane.workingDirectory.value = RedactPath(std::move(pane.workingDirectory.value), options);
                    pane.startingDirectoryFallback = RedactPath(std::move(pane.startingDirectoryFallback), options);
                }
                pane.session.wasCommandRunning = false;
                pane.session.endedUnexpectedly = false;
                pane.session.lastExitCode.reset();
            }
        }
    }
    return result;
}

std::string WorkspaceExporter::Export(const WorkspaceDescriptor& workspace, const WorkspaceExportOptions& options)
{
    return WorkspaceSerializer::Serialize(Prepare(workspace, options));
}

AtomicWriteResult WorkspaceExporter::ExportToFile(
    const WorkspaceDescriptor& workspace,
    const WorkspaceExportOptions& options,
    const std::filesystem::path& path)
{
    return AtomicFileWriter::Write(path, Export(workspace, options));
}
