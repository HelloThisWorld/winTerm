// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Model/WorkspaceDescriptor.h"
#include "../../Shell/Sessions/ShellSessionMetadata.h"

namespace winTerm::Workspaces
{
    struct PaneDirectoryCaptureSources
    {
        std::optional<WorkingDirectoryDescriptor> freshShellIntegration;
        std::optional<WorkingDirectoryDescriptor> lastKnownShellIntegration;
        std::optional<WorkingDirectoryDescriptor> profileStartingDirectory;
        std::optional<WorkingDirectoryDescriptor> processLaunchDirectory;
        std::optional<WorkingDirectoryDescriptor> userHomeDirectory;
    };

    struct WorkspaceCaptureOptions
    {
        bool saveWorkingDirectories{ true };
        bool saveRemoteDirectoryMetadata{ false };
        bool saveCustomTabTitles{ true };
    };

    class WorkspaceCaptureService final
    {
    public:
        static WorkingDirectoryDescriptor SelectWorkingDirectory(const PaneDirectoryCaptureSources& sources);
        static PaneDescriptor CapturePane(
            PaneDescriptor pane,
            const Shell::ShellSessionMetadata* shellMetadata,
            PaneDirectoryCaptureSources directorySources,
            std::string_view capturedAt);
        static WorkspaceDescriptor Capture(
            WorkspaceDescriptor uiSnapshot,
            const WorkspaceCaptureOptions& options,
            std::string_view capturedAt,
            std::string_view captureReason,
            uint64_t generation,
            bool cleanShutdown);
    };
}
