// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Model/WorkspaceDescriptor.h"

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace winTerm::Workspaces
{
    enum class ResolutionStatus
    {
        Exact,
        MatchedFallback,
        SafeDefault,
        Deferred,
        Unavailable,
    };

    struct ProfileCandidate
    {
        std::string id;
        std::string source;
        std::string stableId;
        std::string displayName;
        ShellType shellType{ ShellType::Unknown };
        bool isDefault{ false };
        bool enabled{ true };
    };

    struct ProfileResolution
    {
        ResolutionStatus status{ ResolutionStatus::Unavailable };
        std::optional<ProfileCandidate> profile;
        std::string message;
    };

    class ProfileResolver final
    {
    public:
        static ProfileResolution Resolve(const PaneDescriptor& pane, const std::vector<ProfileCandidate>& profiles);
    };

    struct DirectoryResolutionContext
    {
        std::string profileStartingDirectory;
        std::string userHomeDirectory;
        std::set<std::string, std::less<>> knownWslDistributions;
        bool allowNetworkValidation{ false };
        std::function<bool(std::string_view)> pathExists;
    };

    struct DirectoryResolution
    {
        ResolutionStatus status{ ResolutionStatus::Unavailable };
        WorkingDirectoryKind kind{ WorkingDirectoryKind::Unknown };
        std::string value;
        std::string message;
    };

    class DirectoryResolver final
    {
    public:
        static DirectoryResolution Resolve(const WorkingDirectoryDescriptor& directory, const DirectoryResolutionContext& context);
    };

    struct StringResourceResolution
    {
        ResolutionStatus status{ ResolutionStatus::Unavailable };
        std::string id;
        std::string message;
    };

    class ThemeResolver final
    {
    public:
        static StringResourceResolution Resolve(
            const std::optional<std::string>& paneTheme,
            const std::optional<std::string>& tabTheme,
            std::string_view profileTheme,
            std::string_view globalTheme,
            const std::set<std::string, std::less<>>& availableThemes);
    };

    class FontResolver final
    {
    public:
        static StringResourceResolution Resolve(
            const std::optional<std::string>& paneFont,
            std::string_view profileFont,
            std::string_view globalFont,
            const std::set<std::string, std::less<>>& availableFonts);
    };

    struct MonitorResolutionOptions
    {
        bool restoreMinimizedWindows{ false };
        bool restoreFullscreenWindows{ false };
        double minimumWidthLogical{ 480 };
        double minimumHeightLogical{ 320 };
        double visibleTitleBarLogical{ 64 };
    };

    struct MonitorResolution
    {
        ResolutionStatus status{ ResolutionStatus::Unavailable };
        MonitorDescriptor monitor;
        Rect bounds;
        WindowState windowState{ WindowState::Normal };
        std::vector<std::string> adjustments;
    };

    class MonitorResolver final
    {
    public:
        static MonitorResolution Resolve(
            const WindowDescriptor& window,
            const std::vector<MonitorDescriptor>& monitors,
            const MonitorResolutionOptions& options = {});
    };
}
