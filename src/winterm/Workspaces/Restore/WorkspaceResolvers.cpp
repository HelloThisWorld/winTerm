// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceResolvers.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>

using namespace winTerm::Workspaces;

namespace
{
    template<typename Predicate>
    const ProfileCandidate* FindProfile(const std::vector<ProfileCandidate>& profiles, Predicate&& predicate)
    {
        const auto found = std::find_if(profiles.begin(), profiles.end(), [&](const auto& candidate) {
            return candidate.enabled && predicate(candidate);
        });
        return found == profiles.end() ? nullptr : &*found;
    }

    DirectoryResolution LocalFallback(const DirectoryResolutionContext& context, std::string message)
    {
        if (!context.profileStartingDirectory.empty())
        {
            return { ResolutionStatus::SafeDefault, WorkingDirectoryKind::Windows, context.profileStartingDirectory, std::move(message) };
        }
        if (!context.userHomeDirectory.empty())
        {
            return { ResolutionStatus::SafeDefault, WorkingDirectoryKind::Windows, context.userHomeDirectory, std::move(message) };
        }
        return { ResolutionStatus::Unavailable, WorkingDirectoryKind::Unknown, {}, std::move(message) };
    }

    bool Available(const std::set<std::string, std::less<>>& values, const std::string_view value)
    {
        return !value.empty() && values.contains(value);
    }

    bool MonitorUsable(const MonitorDescriptor& monitor) noexcept
    {
        return std::isfinite(monitor.workArea.x) && std::isfinite(monitor.workArea.y) &&
               std::isfinite(monitor.workArea.width) && std::isfinite(monitor.workArea.height) &&
               monitor.workArea.width > 0 && monitor.workArea.height > 0 &&
               monitor.dpiX >= 48 && monitor.dpiX <= 960 && monitor.dpiY >= 48 && monitor.dpiY <= 960;
    }

    const MonitorDescriptor* PrimaryMonitor(const std::vector<MonitorDescriptor>& monitors)
    {
        const auto primary = std::find_if(monitors.begin(), monitors.end(), [](const auto& monitor) { return monitor.isPrimary && MonitorUsable(monitor); });
        if (primary != monitors.end())
        {
            return &*primary;
        }
        const auto fallback = std::find_if(monitors.begin(), monitors.end(), MonitorUsable);
        return fallback == monitors.end() ? nullptr : &*fallback;
    }

    bool SameResolution(const MonitorDescriptor& left, const MonitorDescriptor& right) noexcept
    {
        return std::abs(left.workArea.width - right.workArea.width) < 1 && std::abs(left.workArea.height - right.workArea.height) < 1;
    }

    Rect BoundsFromNormalized(const Rect& normalized, const Rect& workArea)
    {
        return {
            workArea.x + normalized.x * workArea.width,
            workArea.y + normalized.y * workArea.height,
            normalized.width * workArea.width,
            normalized.height * workArea.height,
        };
    }

    Rect ScaleBoundsForDpi(const WindowDescriptor& window, const MonitorDescriptor& target)
    {
        const auto savedDpiX = std::max<uint32_t>(48, window.monitor.dpiX);
        const auto savedDpiY = std::max<uint32_t>(48, window.monitor.dpiY);
        const auto scaleX = static_cast<double>(target.dpiX) / savedDpiX;
        const auto scaleY = static_cast<double>(target.dpiY) / savedDpiY;
        return {
            target.workArea.x + (window.bounds.x - window.monitor.workArea.x) * scaleX,
            target.workArea.y + (window.bounds.y - window.monitor.workArea.y) * scaleY,
            window.bounds.width * scaleX,
            window.bounds.height * scaleY,
        };
    }

    bool ValidBounds(const Rect& bounds) noexcept
    {
        return std::isfinite(bounds.x) && std::isfinite(bounds.y) && std::isfinite(bounds.width) && std::isfinite(bounds.height) && bounds.width > 0 && bounds.height > 0;
    }

    Rect SafeBounds(Rect bounds, const MonitorDescriptor& monitor, const MonitorResolutionOptions& options, std::vector<std::string>& adjustments)
    {
        const auto& workArea = monitor.workArea;
        const auto dpiScaleX = std::max(0.5, static_cast<double>(monitor.dpiX) / 96.0);
        const auto dpiScaleY = std::max(0.5, static_cast<double>(monitor.dpiY) / 96.0);
        const auto minimumWidth = std::min(workArea.width, options.minimumWidthLogical * dpiScaleX);
        const auto minimumHeight = std::min(workArea.height, options.minimumHeightLogical * dpiScaleY);
        const auto visibleTitle = std::min(workArea.width, options.visibleTitleBarLogical * dpiScaleX);

        if (!ValidBounds(bounds))
        {
            bounds.width = std::min(workArea.width, 960.0 * dpiScaleX);
            bounds.height = std::min(workArea.height, 640.0 * dpiScaleY);
            bounds.x = workArea.x + (workArea.width - bounds.width) / 2;
            bounds.y = workArea.y + (workArea.height - bounds.height) / 2;
            adjustments.emplace_back("Invalid bounds were replaced with a centered safe default.");
        }

        const auto oldBounds = bounds;
        bounds.width = std::clamp(bounds.width, minimumWidth, workArea.width);
        bounds.height = std::clamp(bounds.height, minimumHeight, workArea.height);
        bounds.x = std::clamp(bounds.x, workArea.x - bounds.width + visibleTitle, workArea.x + workArea.width - visibleTitle);
        bounds.y = std::clamp(bounds.y, workArea.y, workArea.y + workArea.height - std::min(bounds.height, visibleTitle));
        if (bounds.width != oldBounds.width || bounds.height != oldBounds.height || bounds.x != oldBounds.x || bounds.y != oldBounds.y)
        {
            adjustments.emplace_back("Window bounds were constrained to the monitor work area.");
        }
        return bounds;
    }
}

ProfileResolution ProfileResolver::Resolve(const PaneDescriptor& pane, const std::vector<ProfileCandidate>& profiles)
{
    if (const auto profile = FindProfile(profiles, [&](const auto& candidate) { return !pane.profileId.empty() && candidate.id == pane.profileId; }))
    {
        return { ResolutionStatus::Exact, *profile, {} };
    }
    if (const auto profile = FindProfile(profiles, [&](const auto& candidate) {
            return !pane.profileSource.empty() && !pane.stableProfileId.empty() && candidate.source == pane.profileSource && candidate.stableId == pane.stableProfileId;
        }))
    {
        return { ResolutionStatus::MatchedFallback, *profile, "The profile was matched by its stable source identifier." };
    }
    if (const auto profile = FindProfile(profiles, [&](const auto& candidate) { return !pane.profileNameFallback.empty() && candidate.displayName == pane.profileNameFallback; }))
    {
        return { ResolutionStatus::MatchedFallback, *profile, "The profile was matched by display name." };
    }
    if (const auto profile = FindProfile(profiles, [&](const auto& candidate) { return pane.shellType != ShellType::Unknown && candidate.shellType == pane.shellType; }))
    {
        return { ResolutionStatus::MatchedFallback, *profile, "The profile was matched by shell type." };
    }
    if (const auto profile = FindProfile(profiles, [](const auto& candidate) { return candidate.isDefault; }))
    {
        return { ResolutionStatus::SafeDefault, *profile, "The saved profile is unavailable; the default profile will be used." };
    }
    if (const auto profile = FindProfile(profiles, [](const auto&) { return true; }))
    {
        return { ResolutionStatus::SafeDefault, *profile, "The saved profile is unavailable; the first enabled profile will be used." };
    }
    return { ResolutionStatus::Unavailable, std::nullopt, "No enabled profile is available for this pane." };
}

DirectoryResolution DirectoryResolver::Resolve(const WorkingDirectoryDescriptor& directory, const DirectoryResolutionContext& context)
{
    if (directory.kind == WorkingDirectoryKind::Remote)
    {
        return LocalFallback(context, "The previous remote directory cannot be restored automatically.");
    }
    if (directory.kind == WorkingDirectoryKind::Wsl)
    {
        const auto distributionAvailable = !directory.distributionId || context.knownWslDistributions.contains(*directory.distributionId);
        if (distributionAvailable && !directory.value.empty() && directory.value.front() == '/')
        {
            return { ResolutionStatus::Exact, WorkingDirectoryKind::Wsl, directory.value, {} };
        }
        return LocalFallback(context, "The saved WSL distribution or directory is unavailable; a local fallback will be used.");
    }
    if (directory.kind == WorkingDirectoryKind::Unc && !context.allowNetworkValidation)
    {
        auto result = LocalFallback(context, "UNC directory validation was deferred to avoid blocking startup.");
        result.status = ResolutionStatus::Deferred;
        return result;
    }

    if ((directory.kind == WorkingDirectoryKind::Windows || directory.kind == WorkingDirectoryKind::Unc) && !directory.value.empty())
    {
        const auto exists = context.pathExists ? context.pathExists : [](const std::string_view value) {
            std::error_code error;
            return std::filesystem::is_directory(std::filesystem::u8path(std::string{ value }), error);
        };
        if (exists(directory.value))
        {
            return { ResolutionStatus::Exact, directory.kind, directory.value, {} };
        }
        auto candidate = std::filesystem::u8path(directory.value);
        while (candidate.has_parent_path() && candidate.parent_path() != candidate)
        {
            candidate = candidate.parent_path();
            const auto candidateText = candidate.u8string();
            const std::string value{ reinterpret_cast<const char*>(candidateText.data()), candidateText.size() };
            if (exists(value))
            {
                return { ResolutionStatus::MatchedFallback, directory.kind, value, "The saved directory is missing; the nearest existing parent will be used." };
            }
        }
    }
    return LocalFallback(context, "The saved directory is unavailable; a safe fallback will be used.");
}

StringResourceResolution ThemeResolver::Resolve(
    const std::optional<std::string>& paneTheme,
    const std::optional<std::string>& tabTheme,
    const std::string_view profileTheme,
    const std::string_view globalTheme,
    const std::set<std::string, std::less<>>& availableThemes)
{
    for (const auto* candidate : { paneTheme ? &*paneTheme : nullptr, tabTheme ? &*tabTheme : nullptr })
    {
        if (candidate && Available(availableThemes, *candidate))
        {
            return { ResolutionStatus::Exact, *candidate, {} };
        }
    }
    for (const auto candidate : { profileTheme, globalTheme, std::string_view{ "winterm.midnight" } })
    {
        if (Available(availableThemes, candidate))
        {
            return { ResolutionStatus::MatchedFallback, std::string{ candidate }, "The requested theme was unavailable; a fallback theme will be used." };
        }
    }
    return { ResolutionStatus::Unavailable, {}, "No available theme could be resolved." };
}

StringResourceResolution FontResolver::Resolve(
    const std::optional<std::string>& paneFont,
    const std::string_view profileFont,
    const std::string_view globalFont,
    const std::set<std::string, std::less<>>& availableFonts)
{
    if (paneFont && Available(availableFonts, *paneFont))
    {
        return { ResolutionStatus::Exact, *paneFont, {} };
    }
    for (const auto candidate : { profileFont, globalFont, std::string_view{ "Cascadia Mono NF" } })
    {
        if (Available(availableFonts, candidate))
        {
            return { ResolutionStatus::MatchedFallback, std::string{ candidate }, "The requested font was unavailable; a fallback font will be used." };
        }
    }
    return { ResolutionStatus::SafeDefault, {}, "The system font fallback will be used." };
}

MonitorResolution MonitorResolver::Resolve(
    const WindowDescriptor& window,
    const std::vector<MonitorDescriptor>& monitors,
    const MonitorResolutionOptions& options)
{
    MonitorResolution result;
    const MonitorDescriptor* target{};
    bool sameMonitor{};

    const auto exactDevice = std::find_if(monitors.begin(), monitors.end(), [&](const auto& monitor) {
        return MonitorUsable(monitor) && !window.monitor.deviceId.empty() && monitor.deviceId == window.monitor.deviceId;
    });
    if (exactDevice != monitors.end())
    {
        target = &*exactDevice;
        sameMonitor = true;
        result.status = ResolutionStatus::Exact;
    }
    if (!target)
    {
        const auto stable = std::find_if(monitors.begin(), monitors.end(), [&](const auto& monitor) {
            return MonitorUsable(monitor) && !window.monitor.stableId.empty() && monitor.stableId == window.monitor.stableId;
        });
        if (stable != monitors.end())
        {
            target = &*stable;
            sameMonitor = true;
            result.status = ResolutionStatus::MatchedFallback;
        }
    }
    if (!target)
    {
        const auto friendly = std::find_if(monitors.begin(), monitors.end(), [&](const auto& monitor) {
            return MonitorUsable(monitor) && !window.monitor.friendlyName.empty() && monitor.friendlyName == window.monitor.friendlyName && SameResolution(monitor, window.monitor);
        });
        if (friendly != monitors.end())
        {
            target = &*friendly;
            sameMonitor = true;
            result.status = ResolutionStatus::MatchedFallback;
        }
    }
    if (!target)
    {
        const auto savedCenterX = window.monitor.workArea.x + window.monitor.workArea.width / 2;
        const auto savedCenterY = window.monitor.workArea.y + window.monitor.workArea.height / 2;
        auto nearestDistance = std::numeric_limits<double>::max();
        for (const auto& monitor : monitors)
        {
            if (!MonitorUsable(monitor))
            {
                continue;
            }
            const auto distance = std::hypot(monitor.workArea.x + monitor.workArea.width / 2 - savedCenterX, monitor.workArea.y + monitor.workArea.height / 2 - savedCenterY);
            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                target = &monitor;
            }
        }
        if (target)
        {
            result.status = ResolutionStatus::MatchedFallback;
        }
    }
    if (!target)
    {
        target = PrimaryMonitor(monitors);
    }
    if (!target)
    {
        result.status = ResolutionStatus::Unavailable;
        result.adjustments.emplace_back("No monitor is available for window restoration.");
        return result;
    }
    if (!sameMonitor && target != PrimaryMonitor(monitors))
    {
        const auto primary = PrimaryMonitor(monitors);
        if (primary)
        {
            target = primary;
            result.status = ResolutionStatus::SafeDefault;
        }
    }

    result.monitor = *target;
    const auto dpiSimilar = std::abs(static_cast<int64_t>(target->dpiX) - window.monitor.dpiX) <= 5 && std::abs(static_cast<int64_t>(target->dpiY) - window.monitor.dpiY) <= 5;
    if (sameMonitor && dpiSimilar)
    {
        result.bounds = window.bounds;
    }
    else if (sameMonitor)
    {
        result.bounds = ScaleBoundsForDpi(window, *target);
        result.adjustments.emplace_back("Window bounds were scaled for the current monitor DPI.");
    }
    else
    {
        result.bounds = BoundsFromNormalized(window.normalizedBounds, target->workArea);
        result.adjustments.emplace_back("The saved monitor is unavailable; normalized bounds were applied to the primary monitor.");
    }
    result.bounds = SafeBounds(result.bounds, *target, options, result.adjustments);

    result.windowState = window.windowState;
    if (result.windowState == WindowState::Minimized && !options.restoreMinimizedWindows)
    {
        result.windowState = WindowState::Normal;
        result.adjustments.emplace_back("A minimized window was restored in the normal state.");
    }
    if (result.windowState == WindowState::Fullscreen && !options.restoreFullscreenWindows)
    {
        result.windowState = WindowState::Normal;
        result.adjustments.emplace_back("Fullscreen restoration is disabled; the window was restored in the normal state.");
    }
    return result;
}
