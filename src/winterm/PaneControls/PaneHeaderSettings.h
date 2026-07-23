// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Actions/DirectedSplitAction.h"
#include "../Design/DensityTokens.h"

#include <cstddef>
#include <string>
#include <vector>

namespace winTerm::PaneControls
{
    enum class PaneHeaderVisibility
    {
        Automatic,
        Always,
        Never,
    };

    enum class AfterSplitFocus
    {
        FocusNewPane,
        KeepCurrentFocus,
    };

    struct PaneHeaderSettings
    {
        PaneHeaderVisibility visibility{ PaneHeaderVisibility::Automatic };
        double height{ Design::DensityTokens::CompactPaneHeaderHeight };
        bool showPaneTitle{ true };
        bool showProfileIcon{ true };
        bool showOverflowButton{ true };
        bool showActiveStatus{ true };
        bool enableUiAnimations{ true };
        Design::InterfaceDensity density{ Design::DensityTokens::Default };
        AfterSplitFocus afterSplit{ AfterSplitFocus::FocusNewPane };
        Actions::SplitProfilePolicy splitProfile{ Actions::SplitProfilePolicy::DefaultProfile };

        void Normalize() noexcept;
        std::vector<std::string> Validate() const;
        bool ShouldShow(size_t paneCount) const noexcept;
        bool HasKeyboardAlternative(bool commandPaletteAvailable) const noexcept;
    };
}
