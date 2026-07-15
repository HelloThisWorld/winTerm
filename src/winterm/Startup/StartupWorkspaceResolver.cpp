// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "StartupWorkspaceResolver.h"

using namespace winTerm::Workspaces;

StartupWorkspaceResolution StartupWorkspaceResolver::Resolve(const WorkspaceSettingsModel& settings, const WorkspaceStore& store)
{
    const auto recovery = WorkspaceRecoveryCoordinator::Inspect(store);
    if (!recovery.previousShutdownWasClean && recovery.latestSnapshot.workspace)
    {
        if (settings.automaticallyRestoreLatestRecoverySnapshot)
        {
            return { StartupWorkspaceAction::RestoreWorkspace, recovery.latestSnapshot.workspace, "Restoring the latest valid recovery snapshot." };
        }
        if (settings.showRecoveryPromptAfterCrash)
        {
            return { StartupWorkspaceAction::ShowRecoveryPrompt, std::nullopt, "The previous winTerm session did not end normally." };
        }
    }

    switch (settings.startupMode)
    {
    case WorkspaceStartupMode::OpenNewSession:
        return { StartupWorkspaceAction::OpenNewSession, std::nullopt, {} };
    case WorkspaceStartupMode::AskEveryTime:
        return { StartupWorkspaceAction::ShowWorkspacePicker, std::nullopt, {} };
    case WorkspaceStartupMode::OpenDefaultWorkspace:
        if (settings.defaultWorkspaceId)
        {
            const auto named = store.LoadNamed(*settings.defaultWorkspaceId);
            if (named.workspace)
            {
                return { StartupWorkspaceAction::RestoreWorkspace, named.workspace, {} };
            }
        }
        [[fallthrough]];
    case WorkspaceStartupMode::RestoreLastSession:
    {
        const auto lastSession = store.LoadLastSession(false);
        if (lastSession.workspace)
        {
            return { StartupWorkspaceAction::RestoreWorkspace, lastSession.workspace, lastSession.message };
        }
        return { StartupWorkspaceAction::OpenNewSession, std::nullopt, "No valid saved workspace is available." };
    }
    }
    return { StartupWorkspaceAction::OpenNewSession, std::nullopt, {} };
}
