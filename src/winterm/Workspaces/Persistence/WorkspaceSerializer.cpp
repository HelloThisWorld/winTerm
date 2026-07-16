// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceSerializer.h"
#include "WorkspaceValidator.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>

using namespace winTerm::Workspaces;

namespace
{
    const Json::Value& RequiredObject(const Json::Value& parent, const char* key)
    {
        const auto& value = parent[key];
        if (!value.isObject())
        {
            throw std::runtime_error(std::string{ "The workspace is missing the required object: " } + key);
        }
        return value;
    }

    const Json::Value& RequiredArray(const Json::Value& parent, const char* key)
    {
        const auto& value = parent[key];
        if (!value.isArray())
        {
            throw std::runtime_error(std::string{ "The workspace is missing the required array: " } + key);
        }
        return value;
    }

    std::string RequiredString(const Json::Value& parent, const char* key)
    {
        const auto& value = parent[key];
        if (!value.isString())
        {
            throw std::runtime_error(std::string{ "The workspace is missing the required string: " } + key);
        }
        return value.asString();
    }

    std::string StringOrDefault(const Json::Value& parent, const char* key, std::string fallback = {})
    {
        const auto& value = parent[key];
        if (value.isNull())
        {
            return fallback;
        }
        if (!value.isString())
        {
            throw std::runtime_error(std::string{ "A workspace field must be a string: " } + key);
        }
        return value.asString();
    }

    std::optional<std::string> OptionalString(const Json::Value& parent, const char* key)
    {
        const auto& value = parent[key];
        if (value.isNull())
        {
            return std::nullopt;
        }
        if (!value.isString())
        {
            throw std::runtime_error(std::string{ "A workspace field must be a string: " } + key);
        }
        return value.asString();
    }

    bool BoolOrDefault(const Json::Value& parent, const char* key, const bool fallback)
    {
        const auto& value = parent[key];
        if (value.isNull())
        {
            return fallback;
        }
        if (!value.isBool())
        {
            throw std::runtime_error(std::string{ "A workspace field must be a boolean: " } + key);
        }
        return value.asBool();
    }

    uint32_t UIntOrDefault(const Json::Value& parent, const char* key, const uint32_t fallback)
    {
        const auto& value = parent[key];
        if (value.isNull())
        {
            return fallback;
        }
        if (!value.isUInt())
        {
            throw std::runtime_error(std::string{ "A workspace field must be an unsigned integer: " } + key);
        }
        return value.asUInt();
    }

    double RequiredFiniteNumber(const Json::Value& parent, const char* key)
    {
        const auto& value = parent[key];
        if (!value.isNumeric())
        {
            throw std::runtime_error(std::string{ "A workspace field must be a finite number: " } + key);
        }
        const auto result = value.asDouble();
        if (!std::isfinite(result))
        {
            throw std::runtime_error(std::string{ "A workspace field must be a finite number: " } + key);
        }
        return result;
    }

    std::optional<double> OptionalFiniteNumber(const Json::Value& parent, const char* key)
    {
        const auto& value = parent[key];
        if (value.isNull())
        {
            return std::nullopt;
        }
        return RequiredFiniteNumber(parent, key);
    }

    void CheckJsonLimits(const Json::Value& value, const size_t depth)
    {
        if (depth > MaximumWorkspaceLayoutDepth + 16)
        {
            throw std::runtime_error("The workspace exceeds the maximum JSON nesting depth.");
        }
        if (value.isString() && value.asString().size() > MaximumWorkspaceStringLength)
        {
            throw std::runtime_error("The workspace contains a string that is too long.");
        }
        if (value.isArray() || value.isObject())
        {
            for (const auto& child : value)
            {
                CheckJsonLimits(child, depth + 1);
            }
        }
    }

    Json::Value RectToJson(const Rect& value)
    {
        Json::Value json{ Json::objectValue };
        json["x"] = value.x;
        json["y"] = value.y;
        json["width"] = value.width;
        json["height"] = value.height;
        return json;
    }

    Rect RectFromJson(const Json::Value& json)
    {
        if (!json.isObject())
        {
            throw std::runtime_error("A workspace bounds field must be an object.");
        }
        return {
            RequiredFiniteNumber(json, "x"),
            RequiredFiniteNumber(json, "y"),
            RequiredFiniteNumber(json, "width"),
            RequiredFiniteNumber(json, "height"),
        };
    }

    Json::Value MonitorToJson(const MonitorDescriptor& monitor)
    {
        Json::Value json{ Json::objectValue };
        json["deviceId"] = monitor.deviceId;
        json["stableId"] = monitor.stableId;
        json["friendlyName"] = monitor.friendlyName;
        json["workArea"] = RectToJson(monitor.workArea);
        json["dpiX"] = monitor.dpiX;
        json["dpiY"] = monitor.dpiY;
        json["isPrimary"] = monitor.isPrimary;
        return json;
    }

    MonitorDescriptor MonitorFromJson(const Json::Value& json)
    {
        if (!json.isObject())
        {
            throw std::runtime_error("A workspace monitor field must be an object.");
        }
        MonitorDescriptor monitor;
        monitor.deviceId = StringOrDefault(json, "deviceId");
        monitor.stableId = StringOrDefault(json, "stableId");
        monitor.friendlyName = StringOrDefault(json, "friendlyName");
        monitor.workArea = RectFromJson(RequiredObject(json, "workArea"));
        monitor.dpiX = UIntOrDefault(json, "dpiX", 96);
        monitor.dpiY = UIntOrDefault(json, "dpiY", 96);
        monitor.isPrimary = BoolOrDefault(json, "isPrimary", false);
        return monitor;
    }

    Json::Value WorkingDirectoryToJson(const WorkingDirectoryDescriptor& directory)
    {
        Json::Value json{ Json::objectValue };
        json["kind"] = std::string{ ToString(directory.kind) };
        json["value"] = directory.value;
        json["source"] = std::string{ ToString(directory.source) };
        json["verifiedAt"] = directory.verifiedAt;
        if (directory.distributionId)
        {
            json["distributionId"] = *directory.distributionId;
        }
        return json;
    }

    WorkingDirectoryDescriptor WorkingDirectoryFromJson(const Json::Value& json)
    {
        if (!json.isObject())
        {
            throw std::runtime_error("A pane working directory must be an object.");
        }
        WorkingDirectoryDescriptor directory;
        const auto kind = WorkingDirectoryKindFromString(StringOrDefault(json, "kind", "unknown"));
        const auto source = WorkingDirectorySourceFromString(StringOrDefault(json, "source", "unknown"));
        if (!kind || !source)
        {
            throw std::runtime_error("A pane working directory uses an unsupported kind or source.");
        }
        directory.kind = *kind;
        directory.value = StringOrDefault(json, "value");
        directory.source = *source;
        directory.verifiedAt = StringOrDefault(json, "verifiedAt");
        directory.distributionId = OptionalString(json, "distributionId");
        return directory;
    }

    Json::Value LayoutToJson(const std::shared_ptr<LayoutNodeDescriptor>& layout, const size_t depth)
    {
        if (!layout || depth > MaximumWorkspaceLayoutDepth)
        {
            throw std::runtime_error("The workspace layout cannot be serialized safely.");
        }
        Json::Value json{ Json::objectValue };
        json["type"] = std::string{ ToString(layout->type) };
        if (layout->type == LayoutNodeType::Pane)
        {
            json["paneId"] = layout->paneId;
        }
        else
        {
            json["orientation"] = std::string{ ToString(layout->orientation) };
            json["ratio"] = std::isfinite(layout->ratio) && layout->ratio >= MinimumSplitRatio && layout->ratio <= MaximumSplitRatio ? layout->ratio : DefaultSplitRatio;
            json["first"] = LayoutToJson(layout->first, depth + 1);
            json["second"] = LayoutToJson(layout->second, depth + 1);
        }
        return json;
    }

    std::shared_ptr<LayoutNodeDescriptor> LayoutFromJson(const Json::Value& json, const size_t depth)
    {
        if (!json.isObject() || depth > MaximumWorkspaceLayoutDepth)
        {
            throw std::runtime_error("The workspace layout is invalid or too deeply nested.");
        }
        const auto type = LayoutNodeTypeFromString(RequiredString(json, "type"));
        if (!type)
        {
            throw std::runtime_error("The workspace layout contains an unsupported node type.");
        }
        if (*type == LayoutNodeType::Pane)
        {
            return LayoutNodeDescriptor::Pane(RequiredString(json, "paneId"));
        }
        const auto orientation = SplitOrientationFromString(RequiredString(json, "orientation"));
        if (!orientation)
        {
            throw std::runtime_error("The workspace layout contains an unsupported split orientation.");
        }
        auto ratio = RequiredFiniteNumber(json, "ratio");
        if (ratio < MinimumSplitRatio || ratio > MaximumSplitRatio)
        {
            ratio = DefaultSplitRatio;
        }
        return LayoutNodeDescriptor::Split(
            *orientation,
            ratio,
            LayoutFromJson(RequiredObject(json, "first"), depth + 1),
            LayoutFromJson(RequiredObject(json, "second"), depth + 1));
    }

    Json::Value PaneToJson(const PaneDescriptor& pane)
    {
        Json::Value json{ Json::objectValue };
        json["id"] = pane.id;
        json["profileId"] = pane.profileId;
        json["profileNameFallback"] = pane.profileNameFallback;
        json["profileSource"] = pane.profileSource;
        json["stableProfileId"] = pane.stableProfileId;
        json["shellType"] = std::string{ ToString(pane.shellType) };
        json["workingDirectory"] = WorkingDirectoryToJson(pane.workingDirectory);
        json["startingDirectoryFallback"] = pane.startingDirectoryFallback;
        json["title"] = pane.title;
        if (pane.themeId)
        {
            json["themeId"] = *pane.themeId;
        }
        Json::Value font{ Json::objectValue };
        if (pane.font.family)
        {
            font["family"] = *pane.font.family;
        }
        if (pane.font.size)
        {
            font["size"] = *pane.font.size;
        }
        if (pane.font.weight)
        {
            font["weight"] = *pane.font.weight;
        }
        json["font"] = std::move(font);
        Json::Value session{ Json::objectValue };
        if (pane.session.lastExitCode)
        {
            session["lastExitCode"] = *pane.session.lastExitCode;
        }
        session["wasCommandRunning"] = pane.session.wasCommandRunning;
        session["endedUnexpectedly"] = pane.session.endedUnexpectedly;
        json["session"] = std::move(session);
        return json;
    }

    PaneDescriptor PaneFromJson(const Json::Value& json)
    {
        if (!json.isObject())
        {
            throw std::runtime_error("A workspace pane must be an object.");
        }
        PaneDescriptor pane;
        pane.id = RequiredString(json, "id");
        pane.profileId = StringOrDefault(json, "profileId");
        pane.profileNameFallback = StringOrDefault(json, "profileNameFallback");
        pane.profileSource = StringOrDefault(json, "profileSource");
        pane.stableProfileId = StringOrDefault(json, "stableProfileId");
        const auto shellType = ShellTypeFromString(StringOrDefault(json, "shellType", "unknown"));
        if (!shellType)
        {
            throw std::runtime_error("A workspace pane uses an unsupported shell type.");
        }
        pane.shellType = *shellType;
        pane.workingDirectory = WorkingDirectoryFromJson(RequiredObject(json, "workingDirectory"));
        pane.startingDirectoryFallback = StringOrDefault(json, "startingDirectoryFallback");
        pane.title = StringOrDefault(json, "title");
        pane.themeId = OptionalString(json, "themeId");
        if (const auto& font = json["font"]; !font.isNull())
        {
            if (!font.isObject())
            {
                throw std::runtime_error("A workspace font override must be an object.");
            }
            pane.font.family = OptionalString(font, "family");
            pane.font.size = OptionalFiniteNumber(font, "size");
            if (!font["weight"].isNull())
            {
                if (!font["weight"].isUInt() || font["weight"].asUInt() > std::numeric_limits<uint16_t>::max())
                {
                    throw std::runtime_error("A workspace font weight must be an unsigned integer.");
                }
                pane.font.weight = static_cast<uint16_t>(font["weight"].asUInt());
            }
        }
        if (const auto& session = json["session"]; !session.isNull())
        {
            if (!session.isObject())
            {
                throw std::runtime_error("A workspace session field must be an object.");
            }
            if (!session["lastExitCode"].isNull())
            {
                if (!session["lastExitCode"].isInt())
                {
                    throw std::runtime_error("A workspace exit code must be an integer.");
                }
                pane.session.lastExitCode = session["lastExitCode"].asInt();
            }
            pane.session.wasCommandRunning = BoolOrDefault(session, "wasCommandRunning", false);
            pane.session.endedUnexpectedly = BoolOrDefault(session, "endedUnexpectedly", false);
        }
        return pane;
    }

    Json::Value TabToJson(const TabDescriptor& tab)
    {
        Json::Value json{ Json::objectValue };
        json["id"] = tab.id;
        json["title"] = tab.title;
        json["customTitle"] = tab.customTitle;
        if (tab.themeId) json["themeId"] = *tab.themeId;
        if (tab.activePaneId) json["activePaneId"] = *tab.activePaneId;
        if (tab.zoomedPaneId) json["zoomedPaneId"] = *tab.zoomedPaneId;
        if (tab.tabColor) json["tabColor"] = *tab.tabColor;
        json["isReadOnly"] = tab.isReadOnly;
        Json::Value panes{ Json::arrayValue };
        for (const auto& pane : tab.panes)
        {
            panes.append(PaneToJson(pane));
        }
        json["panes"] = std::move(panes);
        json["layout"] = LayoutToJson(tab.layout, 1);
        return json;
    }

    TabDescriptor TabFromJson(const Json::Value& json)
    {
        if (!json.isObject())
        {
            throw std::runtime_error("A workspace tab must be an object.");
        }
        TabDescriptor tab;
        tab.id = RequiredString(json, "id");
        tab.title = StringOrDefault(json, "title");
        tab.customTitle = BoolOrDefault(json, "customTitle", false);
        tab.themeId = OptionalString(json, "themeId");
        tab.activePaneId = OptionalString(json, "activePaneId");
        tab.zoomedPaneId = OptionalString(json, "zoomedPaneId");
        tab.tabColor = OptionalString(json, "tabColor");
        tab.isReadOnly = BoolOrDefault(json, "isReadOnly", false);
        const auto& panes = RequiredArray(json, "panes");
        if (panes.size() > MaximumWorkspacePanesPerTab)
        {
            throw std::runtime_error("A workspace tab contains too many panes.");
        }
        tab.panes.reserve(panes.size());
        for (const auto& pane : panes)
        {
            tab.panes.emplace_back(PaneFromJson(pane));
        }
        tab.layout = LayoutFromJson(RequiredObject(json, "layout"), 1);
        return tab;
    }

    Json::Value WindowToJson(const WindowDescriptor& window)
    {
        Json::Value json{ Json::objectValue };
        json["id"] = window.id;
        json["displayName"] = window.displayName;
        json["monitor"] = MonitorToJson(window.monitor);
        json["bounds"] = RectToJson(window.bounds);
        json["normalizedBounds"] = RectToJson(window.normalizedBounds);
        json["windowState"] = std::string{ ToString(window.windowState) };
        json["isAlwaysOnTop"] = window.isAlwaysOnTop;
        if (window.activeTabId) json["activeTabId"] = *window.activeTabId;
        Json::Value tabs{ Json::arrayValue };
        for (const auto& tab : window.tabs)
        {
            tabs.append(TabToJson(tab));
        }
        json["tabs"] = std::move(tabs);
        return json;
    }

    WindowDescriptor WindowFromJson(const Json::Value& json)
    {
        if (!json.isObject())
        {
            throw std::runtime_error("A workspace window must be an object.");
        }
        WindowDescriptor window;
        window.id = RequiredString(json, "id");
        window.displayName = StringOrDefault(json, "displayName");
        window.monitor = MonitorFromJson(RequiredObject(json, "monitor"));
        window.bounds = RectFromJson(RequiredObject(json, "bounds"));
        window.normalizedBounds = RectFromJson(RequiredObject(json, "normalizedBounds"));
        const auto state = WindowStateFromString(StringOrDefault(json, "windowState", "normal"));
        if (!state)
        {
            throw std::runtime_error("A workspace window uses an unsupported state.");
        }
        window.windowState = *state;
        window.isAlwaysOnTop = BoolOrDefault(json, "isAlwaysOnTop", false);
        window.activeTabId = OptionalString(json, "activeTabId");
        const auto& tabs = RequiredArray(json, "tabs");
        if (tabs.size() > MaximumWorkspaceTabsPerWindow)
        {
            throw std::runtime_error("A workspace window contains too many tabs.");
        }
        window.tabs.reserve(tabs.size());
        for (const auto& tab : tabs)
        {
            window.tabs.emplace_back(TabFromJson(tab));
        }
        return window;
    }
}

Json::Value WorkspaceSerializer::ParseJsonDocument(const std::string_view content)
{
    if (content.size() > MaximumWorkspaceFileSize)
    {
        throw std::runtime_error("The workspace file exceeds the maximum allowed size.");
    }
    Json::CharReaderBuilder builder;
    builder["allowComments"] = false;
    builder["allowDroppedNullPlaceholders"] = false;
    builder["allowNumericKeys"] = false;
    builder["allowSingleQuotes"] = false;
    builder["allowSpecialFloats"] = false;
    builder["collectComments"] = false;
    builder["failIfExtra"] = true;
    builder["rejectDupKeys"] = true;
    builder["skipBom"] = true;
    builder["stackLimit"] = static_cast<Json::UInt64>(MaximumWorkspaceLayoutDepth + 16);
    builder["strictRoot"] = true;

    auto reader = std::unique_ptr<Json::CharReader>{ builder.newCharReader() };
    Json::Value root;
    std::string errors;
    if (!reader->parse(content.data(), content.data() + content.size(), &root, &errors))
    {
        throw std::runtime_error("The selected workspace file is not valid JSON.");
    }
    if (!root.isObject())
    {
        throw std::runtime_error("The selected workspace file must contain a JSON object.");
    }
    CheckJsonLimits(root, 0);
    return root;
}

Json::Value WorkspaceSerializer::ToJson(const WorkspaceDescriptor& workspace)
{
    const auto validation = WorkspaceValidator::Validate(workspace);
    if (!validation.IsValid())
    {
        const auto errors = validation.ErrorMessages();
        throw std::runtime_error(errors.empty() ? "The workspace cannot be serialized." : errors.front());
    }

    Json::Value json{ Json::objectValue };
    json["schemaVersion"] = workspace.schemaVersion;
    json["id"] = workspace.id;
    json["name"] = workspace.name;
    json["description"] = workspace.description;
    json["createdAt"] = workspace.createdAt;
    json["updatedAt"] = workspace.updatedAt;
    json["source"] = std::string{ ToString(workspace.source) };
    json["applicationVersion"] = workspace.applicationVersion;
    json["protocolVersion"] = workspace.protocolVersion;
    Json::Value startupBehavior{ Json::objectValue };
    startupBehavior["restoreFocus"] = workspace.startupBehavior.restoreFocus;
    startupBehavior["restoreWindowState"] = workspace.startupBehavior.restoreWindowState;
    json["startupBehavior"] = std::move(startupBehavior);
    if (workspace.activeWindowId) json["activeWindowId"] = *workspace.activeWindowId;
    Json::Value tags{ Json::arrayValue };
    for (const auto& tag : workspace.tags) tags.append(tag);
    json["tags"] = std::move(tags);
    json["isDefault"] = workspace.isDefault;
    if (workspace.lastOpenedAt) json["lastOpenedAt"] = *workspace.lastOpenedAt;
    if (workspace.captureReason) json["captureReason"] = *workspace.captureReason;
    if (workspace.recoveryMetadata)
    {
        Json::Value recovery{ Json::objectValue };
        recovery["generation"] = Json::UInt64{ workspace.recoveryMetadata->generation };
        recovery["cleanShutdown"] = workspace.recoveryMetadata->cleanShutdown;
        recovery["capturedAt"] = workspace.recoveryMetadata->capturedAt;
        json["recoveryMetadata"] = std::move(recovery);
    }
    Json::Value windows{ Json::arrayValue };
    for (const auto& window : workspace.windows)
    {
        windows.append(WindowToJson(window));
    }
    json["windows"] = std::move(windows);
    return json;
}

WorkspaceDescriptor WorkspaceSerializer::FromJson(const Json::Value& json, const bool validate)
{
    if (!json.isObject())
    {
        throw std::runtime_error("The selected workspace file is not a winTerm workspace.");
    }
    WorkspaceDescriptor workspace;
    if (!json["schemaVersion"].isUInt())
    {
        throw std::runtime_error("The workspace schema version is missing or invalid.");
    }
    workspace.schemaVersion = json["schemaVersion"].asUInt();
    workspace.id = RequiredString(json, "id");
    workspace.name = RequiredString(json, "name");
    workspace.description = StringOrDefault(json, "description");
    workspace.createdAt = RequiredString(json, "createdAt");
    workspace.updatedAt = RequiredString(json, "updatedAt");
    const auto source = WorkspaceSourceFromString(StringOrDefault(json, "source", "user"));
    if (!source)
    {
        throw std::runtime_error("The workspace source is not supported.");
    }
    workspace.source = *source;
    workspace.applicationVersion = StringOrDefault(json, "applicationVersion", "0.4.0-dev");
    workspace.protocolVersion = UIntOrDefault(json, "protocolVersion", 1);
    if (const auto& startup = json["startupBehavior"]; !startup.isNull())
    {
        if (!startup.isObject())
        {
            throw std::runtime_error("Workspace startup behavior must be an object.");
        }
        workspace.startupBehavior.restoreFocus = BoolOrDefault(startup, "restoreFocus", true);
        workspace.startupBehavior.restoreWindowState = BoolOrDefault(startup, "restoreWindowState", true);
    }
    workspace.activeWindowId = OptionalString(json, "activeWindowId");
    if (const auto& tags = json["tags"]; !tags.isNull())
    {
        if (!tags.isArray())
        {
            throw std::runtime_error("Workspace tags must be an array.");
        }
        for (const auto& tag : tags)
        {
            if (!tag.isString())
            {
                throw std::runtime_error("Every workspace tag must be a string.");
            }
            workspace.tags.emplace_back(tag.asString());
        }
    }
    workspace.isDefault = BoolOrDefault(json, "isDefault", false);
    workspace.lastOpenedAt = OptionalString(json, "lastOpenedAt");
    workspace.captureReason = OptionalString(json, "captureReason");
    if (const auto& recovery = json["recoveryMetadata"]; !recovery.isNull())
    {
        if (!recovery.isObject() || !recovery["generation"].isUInt64())
        {
            throw std::runtime_error("Workspace recovery metadata is invalid.");
        }
        workspace.recoveryMetadata = RecoveryMetadata{
            recovery["generation"].asUInt64(),
            BoolOrDefault(recovery, "cleanShutdown", true),
            StringOrDefault(recovery, "capturedAt"),
        };
    }
    const auto& windows = RequiredArray(json, "windows");
    if (windows.size() > MaximumWorkspaceWindows)
    {
        throw std::runtime_error("The workspace contains too many windows.");
    }
    workspace.windows.reserve(windows.size());
    for (const auto& window : windows)
    {
        workspace.windows.emplace_back(WindowFromJson(window));
    }
    if (validate)
    {
        const auto validation = WorkspaceValidator::ValidateAndRepair(workspace);
        if (!validation.IsValid())
        {
            const auto errors = validation.ErrorMessages();
            throw std::runtime_error(errors.empty() ? "The selected workspace is invalid." : errors.front());
        }
    }
    return workspace;
}

std::string WorkspaceSerializer::Serialize(const WorkspaceDescriptor& workspace)
{
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["emitUTF8"] = true;
    builder["indentation"] = "  ";
    builder["precision"] = 17;
    return Json::writeString(builder, ToJson(workspace)) + "\n";
}

WorkspaceDescriptor WorkspaceSerializer::Deserialize(const std::string_view content, const bool validate)
{
    return FromJson(ParseJsonDocument(content), validate);
}

std::string WorkspaceSerializer::ReadFile(const std::filesystem::path& path, const size_t maximumSize)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
    {
        throw std::runtime_error("The selected workspace file could not be read.");
    }
    if (size > maximumSize)
    {
        throw std::runtime_error("The workspace file exceeds the maximum allowed size.");
    }
    std::ifstream input{ path, std::ios::binary };
    if (!input)
    {
        throw std::runtime_error("The selected workspace file could not be read.");
    }
    std::string content(static_cast<size_t>(size), '\0');
    if (!content.empty())
    {
        input.read(content.data(), static_cast<std::streamsize>(content.size()));
        if (input.gcount() != static_cast<std::streamsize>(content.size()))
        {
            throw std::runtime_error("The selected workspace file could not be read.");
        }
    }
    return content;
}
