// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Model/WorkspaceDescriptor.h"
#include "../Persistence/AtomicFileWriter.h"

#include <filesystem>
#include <json/json.h>
#include <optional>
#include <vector>

namespace winTerm::Workspaces
{
    enum class WorkspaceImportStatus
    {
        ReadyForConfirmation,
        Rejected,
        NewerSchemaPreserved,
    };

    struct WorkspaceImportSummary
    {
        std::string name;
        size_t windowCount{};
        size_t tabCount{};
        size_t paneCount{};
        std::vector<std::string> referencedProfiles;
        std::vector<std::string> referencedThemes;
        std::vector<std::string> referencedFonts;
        std::vector<std::string> ignoredFields;
        std::vector<std::string> securityWarnings;
    };

    struct WorkspaceImportResult
    {
        WorkspaceImportStatus status{ WorkspaceImportStatus::Rejected };
        std::optional<WorkspaceDescriptor> workspace;
        WorkspaceImportSummary summary;
        std::string message;
    };

    class WorkspaceImportSanitizer final
    {
    public:
        static std::vector<std::string> Inspect(const Json::Value& document);
        static WorkspaceDescriptor Sanitize(WorkspaceDescriptor workspace);
    };

    class WorkspaceImporter final
    {
    public:
        static WorkspaceImportResult InspectFile(const std::filesystem::path& path);
        static WorkspaceImportResult Inspect(std::string_view content);
    };

    struct WorkspaceExportOptions
    {
        bool includeWorkingDirectories{ true };
        bool redactUserHome{ true };
        bool includeMonitorMetadata{ true };
        bool includeDescriptionAndTags{ true };
        bool includeCustomTitles{ true };
        std::string userHomeDirectory;
    };

    class WorkspaceExporter final
    {
    public:
        static WorkspaceDescriptor Prepare(const WorkspaceDescriptor& workspace, const WorkspaceExportOptions& options);
        static std::string Export(const WorkspaceDescriptor& workspace, const WorkspaceExportOptions& options);
        static AtomicWriteResult ExportToFile(const WorkspaceDescriptor& workspace, const WorkspaceExportOptions& options, const std::filesystem::path& path);
    };
}
