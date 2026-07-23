// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "PaneHeaderSettings.h"

#include <cstddef>
#include <string>

namespace winTerm::PaneControls
{
    enum class PaneExecutionState
    {
        Idle,
        Running,
        Failed,
    };

    struct PaneTitleSources
    {
        std::string userTitle;
        std::string shellTitle;
        std::string profileName;
        std::string shellType;
    };

    struct PaneHeaderState
    {
        size_t paneCount{ 1 };
        bool focused{ false };
        bool readOnly{ false };
        PaneExecutionState execution{ PaneExecutionState::Idle };
        PaneTitleSources titles;
        std::string profileIcon;
    };

    struct PaneHeaderPresentation
    {
        bool visible{ false };
        double height{};
        std::string title;
        std::string accessibleName;
        std::string iconAccessibleName;
        std::string overflowAccessibleName{ "Open pane menu" };
        std::string statusText;
        bool showProfileIcon{ false };
        bool showOverflowButton{ false };
    };

    class PaneHeaderViewModel
    {
    public:
        static PaneHeaderPresentation Build(
            const PaneHeaderState& state,
            const PaneHeaderSettings& settings);
        static std::string ResolveTitle(const PaneTitleSources& sources);
        static std::string SanitizeTitle(std::string title);
    };
}
