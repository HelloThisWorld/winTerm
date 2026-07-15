// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>

namespace winTerm::Workspaces
{
    enum class WorkspaceStartupMode
    {
        OpenNewSession,
        RestoreLastSession,
        OpenDefaultWorkspace,
        AskEveryTime,
    };

    enum class WorkspaceExitMode
    {
        AlwaysSave,
        AskBeforeReplacingLastSession,
        DoNotSave,
    };

    struct WorkspaceSettingsModel
    {
        WorkspaceStartupMode startupMode{ WorkspaceStartupMode::RestoreLastSession };
        WorkspaceExitMode exitMode{ WorkspaceExitMode::AlwaysSave };
        std::optional<std::string> defaultWorkspaceId;
        bool restoreActiveWindow{ true };
        bool restoreActiveTab{ true };
        bool restoreFocusedPane{ true };
        bool restoreFullscreenState{ false };
        bool restoreMinimizedWindows{ false };
        bool autosaveEnabled{ true };
        std::chrono::milliseconds autosaveDebounce{ 750 };
        std::chrono::seconds crashSnapshotInterval{ 30 };
        size_t snapshotRetentionCount{ 3 };
        bool saveWorkingDirectories{ true };
        bool saveRemoteDirectoryMetadata{ false };
        bool saveCustomTabTitles{ true };
        bool showRecoveryPromptAfterCrash{ true };
        bool automaticallyRestoreLatestRecoverySnapshot{ false };
        bool keepRecoverySnapshots{ true };
        bool redactUserHomeWhenExporting{ true };
        bool includeMonitorMetadataWhenExporting{ true };
        bool includeWorkingDirectoriesWhenExporting{ true };
    };
}
