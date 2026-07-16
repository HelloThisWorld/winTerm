// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceCaptureService.h"

#include <algorithm>
#include <limits>

using namespace winTerm::Workspaces;

namespace
{
    WorkingDirectoryKind ToWorkspaceDirectoryKind(const winTerm::Shell::CurrentDirectoryKind value) noexcept
    {
        switch (value)
        {
        case winTerm::Shell::CurrentDirectoryKind::LocalWindows: return WorkingDirectoryKind::Windows;
        case winTerm::Shell::CurrentDirectoryKind::Unc: return WorkingDirectoryKind::Unc;
        case winTerm::Shell::CurrentDirectoryKind::Wsl: return WorkingDirectoryKind::Wsl;
        case winTerm::Shell::CurrentDirectoryKind::Remote: return WorkingDirectoryKind::Remote;
        default: return WorkingDirectoryKind::Unknown;
        }
    }

    winTerm::Workspaces::ShellType ToWorkspaceShellType(const winTerm::Shell::ShellType value) noexcept
    {
        switch (value)
        {
        case winTerm::Shell::ShellType::PowerShell: return winTerm::Workspaces::ShellType::PowerShell;
        case winTerm::Shell::ShellType::WindowsPowerShell: return winTerm::Workspaces::ShellType::WindowsPowerShell;
        case winTerm::Shell::ShellType::CommandPrompt: return winTerm::Workspaces::ShellType::CommandPrompt;
        case winTerm::Shell::ShellType::Wsl: return winTerm::Workspaces::ShellType::Wsl;
        case winTerm::Shell::ShellType::GitBash: return winTerm::Workspaces::ShellType::GitBash;
        case winTerm::Shell::ShellType::Ssh: return winTerm::Workspaces::ShellType::Ssh;
        default: return winTerm::Workspaces::ShellType::Unknown;
        }
    }
}

WorkingDirectoryDescriptor WorkspaceCaptureService::SelectWorkingDirectory(const PaneDirectoryCaptureSources& sources)
{
    for (const auto* candidate : {
             &sources.freshShellIntegration,
             &sources.lastKnownShellIntegration,
             &sources.profileStartingDirectory,
             &sources.processLaunchDirectory,
             &sources.userHomeDirectory })
    {
        if (*candidate && !(*candidate)->value.empty())
        {
            return **candidate;
        }
    }
    return {};
}

PaneDescriptor WorkspaceCaptureService::CapturePane(
    PaneDescriptor pane,
    const Shell::ShellSessionMetadata* shellMetadata,
    PaneDirectoryCaptureSources directorySources,
    const std::string_view capturedAt)
{
    if (shellMetadata)
    {
        if (pane.profileId.empty() && !shellMetadata->profileId.empty())
        {
            pane.profileId = til::u16u8(shellMetadata->profileId);
        }
        if (pane.shellType == ShellType::Unknown)
        {
            pane.shellType = ToWorkspaceShellType(shellMetadata->shellType);
        }
        if (!shellMetadata->currentDirectory.value.empty())
        {
            directorySources.freshShellIntegration = WorkingDirectoryDescriptor{
                ToWorkspaceDirectoryKind(shellMetadata->currentDirectory.kind),
                til::u16u8(shellMetadata->currentDirectory.value),
                WorkingDirectorySource::ShellIntegration,
                std::string{ capturedAt },
                std::nullopt,
            };
        }
        if (shellMetadata->lastExitCode)
        {
            pane.session.lastExitCode = static_cast<int32_t>(std::min<uint32_t>(
                *shellMetadata->lastExitCode,
                static_cast<uint32_t>(std::numeric_limits<int32_t>::max())));
        }
        pane.session.wasCommandRunning = shellMetadata->commandState == Shell::CommandExecutionState::Running;
    }
    pane.workingDirectory = SelectWorkingDirectory(directorySources);
    return pane;
}

WorkspaceDescriptor WorkspaceCaptureService::Capture(
    WorkspaceDescriptor uiSnapshot,
    const WorkspaceCaptureOptions& options,
    const std::string_view capturedAt,
    const std::string_view captureReason,
    const uint64_t generation,
    const bool cleanShutdown)
{
    uiSnapshot.schemaVersion = WorkspaceSchemaVersion;
    uiSnapshot.source = WorkspaceSource::Runtime;
    uiSnapshot.updatedAt = std::string{ capturedAt };
    if (uiSnapshot.createdAt.empty())
    {
        uiSnapshot.createdAt = uiSnapshot.updatedAt;
    }
    uiSnapshot.captureReason = std::string{ captureReason };
    uiSnapshot.recoveryMetadata = RecoveryMetadata{ generation, cleanShutdown, std::string{ capturedAt } };

    for (auto& window : uiSnapshot.windows)
    {
        for (auto& tab : window.tabs)
        {
            if (!options.saveCustomTabTitles && tab.customTitle)
            {
                tab.title.clear();
                tab.customTitle = false;
            }
            for (auto& pane : tab.panes)
            {
                if (!options.saveCustomTabTitles)
                {
                    pane.title.clear();
                }
                if (!options.saveWorkingDirectories || (pane.workingDirectory.kind == WorkingDirectoryKind::Remote && !options.saveRemoteDirectoryMetadata))
                {
                    pane.workingDirectory = {};
                    pane.startingDirectoryFallback.clear();
                }
            }
        }
    }
    return uiSnapshot;
}
