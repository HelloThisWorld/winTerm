// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace winTerm::Workspaces
{
    inline constexpr uint32_t WorkspaceSchemaVersion{ 2 };
    inline constexpr uint32_t DockingModelVersion{ 1 };
    inline constexpr size_t MaximumWorkspaceFileSize{ 5 * 1024 * 1024 };
    inline constexpr size_t MaximumWorkspaceWindows{ 32 };
    inline constexpr size_t MaximumWorkspaceTabsPerWindow{ 128 };
    inline constexpr size_t MaximumWorkspacePanesPerTab{ 64 };
    inline constexpr size_t MaximumWorkspacePanes{ 512 };
    inline constexpr size_t MaximumWorkspaceLayoutDepth{ 32 };
    inline constexpr size_t MaximumWorkspaceStringLength{ 16 * 1024 };
    inline constexpr double MinimumSplitRatio{ 0.05 };
    inline constexpr double MaximumSplitRatio{ 0.95 };
    inline constexpr double DefaultSplitRatio{ 0.5 };

    enum class WorkspaceSource
    {
        Runtime,
        User,
        Imported,
        Recovery,
    };

    enum class WindowState
    {
        Normal,
        Maximized,
        Fullscreen,
        Minimized,
    };

    enum class LayoutNodeType
    {
        Pane,
        Split,
        EmptySlot,
    };

    enum class SplitOrientation
    {
        Horizontal,
        Vertical,
    };

    enum class ShellType
    {
        Unknown,
        PowerShell,
        WindowsPowerShell,
        CommandPrompt,
        Wsl,
        GitBash,
        Ssh,
    };

    enum class WorkingDirectoryKind
    {
        Unknown,
        Windows,
        Unc,
        Wsl,
        Remote,
    };

    enum class WorkingDirectorySource
    {
        Unknown,
        ShellIntegration,
        Profile,
        Launch,
        Fallback,
    };

    struct Rect
    {
        double x{};
        double y{};
        double width{};
        double height{};
    };

    struct MonitorDescriptor
    {
        std::string deviceId;
        std::string stableId;
        std::string friendlyName;
        Rect workArea;
        uint32_t dpiX{ 96 };
        uint32_t dpiY{ 96 };
        bool isPrimary{ false };
    };

    struct WorkingDirectoryDescriptor
    {
        WorkingDirectoryKind kind{ WorkingDirectoryKind::Unknown };
        std::string value;
        WorkingDirectorySource source{ WorkingDirectorySource::Unknown };
        std::string verifiedAt;
        std::optional<std::string> distributionId;
    };

    struct FontOverride
    {
        std::optional<std::string> family;
        std::optional<double> size;
        std::optional<uint16_t> weight;
    };

    struct SessionDescriptor
    {
        std::optional<int32_t> lastExitCode;
        bool wasCommandRunning{ false };
        bool endedUnexpectedly{ false };
    };

    struct PaneDescriptor
    {
        std::string id;
        std::string profileId;
        std::string profileNameFallback;
        std::string profileSource;
        std::string stableProfileId;
        ShellType shellType{ ShellType::Unknown };
        WorkingDirectoryDescriptor workingDirectory;
        std::string startingDirectoryFallback;
        std::string title;
        std::optional<std::string> themeId;
        FontOverride font;
        SessionDescriptor session;
    };

    struct LayoutNodeDescriptor
    {
        LayoutNodeType type{ LayoutNodeType::Pane };
        std::string paneId;
        std::string slotId;
        std::optional<std::string> preferredProfileId;
        SplitOrientation orientation{ SplitOrientation::Vertical };
        double ratio{ DefaultSplitRatio };
        std::shared_ptr<LayoutNodeDescriptor> first;
        std::shared_ptr<LayoutNodeDescriptor> second;

        static std::shared_ptr<LayoutNodeDescriptor> Pane(std::string paneId);
        static std::shared_ptr<LayoutNodeDescriptor> Split(
            SplitOrientation orientation,
            double ratio,
            std::shared_ptr<LayoutNodeDescriptor> first,
            std::shared_ptr<LayoutNodeDescriptor> second);
        static std::shared_ptr<LayoutNodeDescriptor> EmptySlot(
            std::string slotId,
            std::optional<std::string> preferredProfileId = std::nullopt);
    };

    struct TabDescriptor
    {
        std::string id;
        std::string title;
        bool customTitle{ false };
        std::optional<std::string> themeId;
        std::optional<std::string> activePaneId;
        std::optional<std::string> zoomedPaneId;
        std::optional<std::string> tabColor;
        bool isReadOnly{ false };
        std::vector<PaneDescriptor> panes;
        std::shared_ptr<LayoutNodeDescriptor> layout;
    };

    struct WindowDescriptor
    {
        std::string id;
        std::string displayName;
        MonitorDescriptor monitor;
        Rect bounds;
        Rect normalizedBounds;
        WindowState windowState{ WindowState::Normal };
        bool isAlwaysOnTop{ false };
        std::optional<std::string> activeTabId;
        std::vector<TabDescriptor> tabs;
    };

    struct WorkspaceStartupBehavior
    {
        bool restoreFocus{ true };
        bool restoreWindowState{ true };
    };

    struct RecoveryMetadata
    {
        uint64_t generation{};
        bool cleanShutdown{ true };
        std::string capturedAt;
    };

    struct LayoutHistoryMetadata
    {
        uint32_t limit{ 20 };
        uint64_t lastSequence{};
    };

    struct WorkspaceDescriptor
    {
        uint32_t schemaVersion{ WorkspaceSchemaVersion };
        std::string id;
        std::string name;
        std::string description;
        std::string createdAt;
        std::string updatedAt;
        WorkspaceSource source{ WorkspaceSource::User };
        std::string applicationVersion{ "1.0.2" };
        uint32_t protocolVersion{ 1 };
        uint32_t dockingModelVersion{ DockingModelVersion };
        WorkspaceStartupBehavior startupBehavior;
        std::optional<std::string> activeWindowId;
        std::vector<std::string> tags;
        bool isDefault{ false };
        std::optional<std::string> lastOpenedAt;
        std::optional<std::string> captureReason;
        std::optional<RecoveryMetadata> recoveryMetadata;
        std::optional<LayoutHistoryMetadata> layoutHistoryMetadata;
        std::vector<WindowDescriptor> windows;

        size_t TabCount() const noexcept;
        size_t PaneCount() const noexcept;
    };

    std::string_view ToString(WorkspaceSource value) noexcept;
    std::string_view ToString(WindowState value) noexcept;
    std::string_view ToString(LayoutNodeType value) noexcept;
    std::string_view ToString(SplitOrientation value) noexcept;
    std::string_view ToString(ShellType value) noexcept;
    std::string_view ToString(WorkingDirectoryKind value) noexcept;
    std::string_view ToString(WorkingDirectorySource value) noexcept;

    std::optional<WorkspaceSource> WorkspaceSourceFromString(std::string_view value) noexcept;
    std::optional<WindowState> WindowStateFromString(std::string_view value) noexcept;
    std::optional<LayoutNodeType> LayoutNodeTypeFromString(std::string_view value) noexcept;
    std::optional<SplitOrientation> SplitOrientationFromString(std::string_view value) noexcept;
    std::optional<ShellType> ShellTypeFromString(std::string_view value) noexcept;
    std::optional<WorkingDirectoryKind> WorkingDirectoryKindFromString(std::string_view value) noexcept;
    std::optional<WorkingDirectorySource> WorkingDirectorySourceFromString(std::string_view value) noexcept;
}
