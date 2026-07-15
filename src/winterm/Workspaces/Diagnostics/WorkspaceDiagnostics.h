// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Persistence/WorkspaceStore.h"

namespace winTerm::Workspaces
{
    struct WorkspaceDiagnosticsSnapshot
    {
        std::filesystem::path storageLocation;
        uint32_t schemaVersion{ WorkspaceSchemaVersion };
        WorkspaceLoadStatus lastSessionHealth{ WorkspaceLoadStatus::Missing };
        std::string lastSuccessfulSave;
        bool lastShutdownClean{ true };
        size_t snapshotCount{};
        size_t namedWorkspaceCount{};
        size_t quarantinedFileCount{};
        std::optional<uint64_t> lastRestoreDurationMilliseconds;
        size_t lastRestoreAdjustments{};
        bool autosaveEnabled{ true };
        bool dirty{ false };
        uint64_t saveGeneration{};
    };

    class WorkspaceDiagnostics final
    {
    public:
        static WorkspaceDiagnosticsSnapshot Capture(
            const WorkspaceStore& store,
            bool autosaveEnabled,
            bool dirty,
            uint64_t generation,
            std::optional<uint64_t> lastRestoreDurationMilliseconds = std::nullopt,
            size_t lastRestoreAdjustments = 0);
        static std::string CopySafeText(const WorkspaceDiagnosticsSnapshot& diagnostics);
        static std::string RedactPath(std::string_view path);
    };
}
