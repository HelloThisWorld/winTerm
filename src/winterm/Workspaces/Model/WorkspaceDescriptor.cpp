// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceDescriptor.h"

#include <utility>

using namespace winTerm::Workspaces;

namespace
{
    template<typename T, size_t Size>
    std::optional<T> EnumFromString(const std::string_view value, const std::pair<T, std::string_view> (&values)[Size]) noexcept
    {
        for (const auto& [candidate, text] : values)
        {
            if (value == text)
            {
                return candidate;
            }
        }
        return std::nullopt;
    }
}

std::shared_ptr<LayoutNodeDescriptor> LayoutNodeDescriptor::Pane(std::string paneId)
{
    auto node = std::make_shared<LayoutNodeDescriptor>();
    node->type = LayoutNodeType::Pane;
    node->paneId = std::move(paneId);
    return node;
}

std::shared_ptr<LayoutNodeDescriptor> LayoutNodeDescriptor::Split(
    const SplitOrientation orientation,
    const double ratio,
    std::shared_ptr<LayoutNodeDescriptor> first,
    std::shared_ptr<LayoutNodeDescriptor> second)
{
    auto node = std::make_shared<LayoutNodeDescriptor>();
    node->type = LayoutNodeType::Split;
    node->orientation = orientation;
    node->ratio = ratio;
    node->first = std::move(first);
    node->second = std::move(second);
    return node;
}

size_t WorkspaceDescriptor::TabCount() const noexcept
{
    size_t result{};
    for (const auto& window : windows)
    {
        result += window.tabs.size();
    }
    return result;
}

size_t WorkspaceDescriptor::PaneCount() const noexcept
{
    size_t result{};
    for (const auto& window : windows)
    {
        for (const auto& tab : window.tabs)
        {
            result += tab.panes.size();
        }
    }
    return result;
}

std::string_view winTerm::Workspaces::ToString(const WorkspaceSource value) noexcept
{
    switch (value)
    {
    case WorkspaceSource::Runtime: return "runtime";
    case WorkspaceSource::User: return "user";
    case WorkspaceSource::Imported: return "imported";
    case WorkspaceSource::Recovery: return "recovery";
    }
    return "user";
}

std::string_view winTerm::Workspaces::ToString(const WindowState value) noexcept
{
    switch (value)
    {
    case WindowState::Normal: return "normal";
    case WindowState::Maximized: return "maximized";
    case WindowState::Fullscreen: return "fullscreen";
    case WindowState::Minimized: return "minimized";
    }
    return "normal";
}

std::string_view winTerm::Workspaces::ToString(const LayoutNodeType value) noexcept
{
    return value == LayoutNodeType::Split ? "split" : "pane";
}

std::string_view winTerm::Workspaces::ToString(const SplitOrientation value) noexcept
{
    return value == SplitOrientation::Horizontal ? "horizontal" : "vertical";
}

std::string_view winTerm::Workspaces::ToString(const ShellType value) noexcept
{
    switch (value)
    {
    case ShellType::PowerShell: return "powershell";
    case ShellType::WindowsPowerShell: return "windowsPowerShell";
    case ShellType::CommandPrompt: return "cmd";
    case ShellType::Wsl: return "wsl";
    case ShellType::GitBash: return "gitBash";
    case ShellType::Ssh: return "ssh";
    default: return "unknown";
    }
}

std::string_view winTerm::Workspaces::ToString(const WorkingDirectoryKind value) noexcept
{
    switch (value)
    {
    case WorkingDirectoryKind::Windows: return "windows";
    case WorkingDirectoryKind::Unc: return "unc";
    case WorkingDirectoryKind::Wsl: return "wsl";
    case WorkingDirectoryKind::Remote: return "remote";
    default: return "unknown";
    }
}

std::string_view winTerm::Workspaces::ToString(const WorkingDirectorySource value) noexcept
{
    switch (value)
    {
    case WorkingDirectorySource::ShellIntegration: return "shellIntegration";
    case WorkingDirectorySource::Profile: return "profile";
    case WorkingDirectorySource::Launch: return "launch";
    case WorkingDirectorySource::Fallback: return "fallback";
    default: return "unknown";
    }
}

std::optional<WorkspaceSource> winTerm::Workspaces::WorkspaceSourceFromString(const std::string_view value) noexcept
{
    static constexpr std::pair<WorkspaceSource, std::string_view> values[]{
        std::pair{ WorkspaceSource::Runtime, std::string_view{ "runtime" } },
        std::pair{ WorkspaceSource::User, std::string_view{ "user" } },
        std::pair{ WorkspaceSource::Imported, std::string_view{ "imported" } },
        std::pair{ WorkspaceSource::Recovery, std::string_view{ "recovery" } },
    };
    return EnumFromString(value, values);
}

std::optional<WindowState> winTerm::Workspaces::WindowStateFromString(const std::string_view value) noexcept
{
    static constexpr std::pair<WindowState, std::string_view> values[]{
        std::pair{ WindowState::Normal, std::string_view{ "normal" } },
        std::pair{ WindowState::Maximized, std::string_view{ "maximized" } },
        std::pair{ WindowState::Fullscreen, std::string_view{ "fullscreen" } },
        std::pair{ WindowState::Minimized, std::string_view{ "minimized" } },
    };
    return EnumFromString(value, values);
}

std::optional<LayoutNodeType> winTerm::Workspaces::LayoutNodeTypeFromString(const std::string_view value) noexcept
{
    static constexpr std::pair<LayoutNodeType, std::string_view> values[]{
        std::pair{ LayoutNodeType::Pane, std::string_view{ "pane" } },
        std::pair{ LayoutNodeType::Split, std::string_view{ "split" } },
    };
    return EnumFromString(value, values);
}

std::optional<SplitOrientation> winTerm::Workspaces::SplitOrientationFromString(const std::string_view value) noexcept
{
    static constexpr std::pair<SplitOrientation, std::string_view> values[]{
        std::pair{ SplitOrientation::Horizontal, std::string_view{ "horizontal" } },
        std::pair{ SplitOrientation::Vertical, std::string_view{ "vertical" } },
    };
    return EnumFromString(value, values);
}

std::optional<ShellType> winTerm::Workspaces::ShellTypeFromString(const std::string_view value) noexcept
{
    static constexpr std::pair<ShellType, std::string_view> values[]{
        std::pair{ ShellType::Unknown, std::string_view{ "unknown" } },
        std::pair{ ShellType::PowerShell, std::string_view{ "powershell" } },
        std::pair{ ShellType::WindowsPowerShell, std::string_view{ "windowsPowerShell" } },
        std::pair{ ShellType::CommandPrompt, std::string_view{ "cmd" } },
        std::pair{ ShellType::Wsl, std::string_view{ "wsl" } },
        std::pair{ ShellType::GitBash, std::string_view{ "gitBash" } },
        std::pair{ ShellType::Ssh, std::string_view{ "ssh" } },
    };
    return EnumFromString(value, values);
}

std::optional<WorkingDirectoryKind> winTerm::Workspaces::WorkingDirectoryKindFromString(const std::string_view value) noexcept
{
    static constexpr std::pair<WorkingDirectoryKind, std::string_view> values[]{
        std::pair{ WorkingDirectoryKind::Unknown, std::string_view{ "unknown" } },
        std::pair{ WorkingDirectoryKind::Windows, std::string_view{ "windows" } },
        std::pair{ WorkingDirectoryKind::Unc, std::string_view{ "unc" } },
        std::pair{ WorkingDirectoryKind::Wsl, std::string_view{ "wsl" } },
        std::pair{ WorkingDirectoryKind::Remote, std::string_view{ "remote" } },
    };
    return EnumFromString(value, values);
}

std::optional<WorkingDirectorySource> winTerm::Workspaces::WorkingDirectorySourceFromString(const std::string_view value) noexcept
{
    static constexpr std::pair<WorkingDirectorySource, std::string_view> values[]{
        std::pair{ WorkingDirectorySource::Unknown, std::string_view{ "unknown" } },
        std::pair{ WorkingDirectorySource::ShellIntegration, std::string_view{ "shellIntegration" } },
        std::pair{ WorkingDirectorySource::Profile, std::string_view{ "profile" } },
        std::pair{ WorkingDirectorySource::Launch, std::string_view{ "launch" } },
        std::pair{ WorkingDirectorySource::Fallback, std::string_view{ "fallback" } },
    };
    return EnumFromString(value, values);
}
