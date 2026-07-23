// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PaneHeaderViewModel.h"

#include <algorithm>
#include <cctype>

using namespace winTerm::PaneControls;

namespace
{
    bool LooksLikeAbsolutePath(const std::string& value)
    {
        return (value.size() >= 3 &&
                std::isalpha(static_cast<unsigned char>(value[0])) &&
                value[1] == ':' &&
                (value[2] == '\\' || value[2] == '/')) ||
               (!value.empty() && (value[0] == '/' || value[0] == '\\'));
    }

    std::string ResolveAccessibleTitle(const PaneTitleSources& sources)
    {
        for (const auto* candidate : {
                 &sources.userTitle,
                 &sources.shellTitle,
                 &sources.profileName,
                 &sources.shellType })
        {
            if (!candidate->empty())
            {
                return PaneHeaderViewModel::SanitizeTitle(*candidate);
            }
        }
        return "Terminal";
    }
}

PaneHeaderPresentation PaneHeaderViewModel::Build(
    const PaneHeaderState& state,
    const PaneHeaderSettings& settings)
{
    PaneHeaderPresentation result;
    result.visible = settings.ShouldShow(state.paneCount);
    result.height = settings.height;
    const auto accessibleTitle = ResolveAccessibleTitle(state.titles);
    result.title = settings.showPaneTitle ? ResolveTitle(state.titles) : std::string{};

    if (state.execution == PaneExecutionState::Running)
    {
        result.statusText = "Running command";
    }
    else if (state.execution == PaneExecutionState::Failed)
    {
        result.statusText = "Last command failed";
    }
    if (state.readOnly)
    {
        if (!result.statusText.empty())
        {
            result.statusText += ". ";
        }
        result.statusText += "Read-only";
    }

    result.accessibleName = accessibleTitle + " pane";
    result.accessibleName += state.focused ? ". Focused" : ". Not focused";
    if (!result.statusText.empty())
    {
        result.accessibleName += ". " + result.statusText;
    }
    result.iconAccessibleName = accessibleTitle + " pane";
    result.showProfileIcon = settings.showProfileIcon && !state.profileIcon.empty();
    result.showOverflowButton = settings.showOverflowButton;
    return result;
}

std::string PaneHeaderViewModel::ResolveTitle(const PaneTitleSources& sources)
{
    for (const auto* candidate : {
             &sources.userTitle,
             &sources.shellTitle,
             &sources.profileName,
             &sources.shellType })
    {
        if (!candidate->empty())
        {
            auto title = SanitizeTitle(*candidate);
            constexpr size_t MaximumTitleLength = 80;
            if (title.size() > MaximumTitleLength)
            {
                title.resize(MaximumTitleLength - 3);
                title += "...";
            }
            return title;
        }
    }
    return "Terminal";
}

std::string PaneHeaderViewModel::SanitizeTitle(std::string title)
{
    if (LooksLikeAbsolutePath(title))
    {
        const auto separator = title.find_last_of("\\/");
        title = separator == std::string::npos ? "Terminal" : title.substr(separator + 1);
    }
    return title;
}
