// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Settings/Workspaces/WorkspaceSettingsModel.h"
#include "../Workspaces/Persistence/WorkspaceStore.h"
#include "../Workspaces/Runtime/WorkspaceRuntime.h"

namespace winTerm::Workspaces
{
    enum class StartupWorkspaceAction
    {
        OpenNewSession,
        RestoreWorkspace,
        ShowWorkspacePicker,
        ShowRecoveryPrompt,
    };

    struct StartupWorkspaceResolution
    {
        StartupWorkspaceAction action{ StartupWorkspaceAction::OpenNewSession };
        std::optional<WorkspaceDescriptor> workspace;
        std::string message;
    };

    class StartupWorkspaceResolver final
    {
    public:
        static StartupWorkspaceResolution Resolve(const WorkspaceSettingsModel& settings, const WorkspaceStore& store);
    };
}
