// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Actions/DirectedSplitAction.h"

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
        double height{ 26.0 };
        bool showPaneTitle{ true };
        bool showProfileIcon{ true };
        bool showOverflowButton{ true };
        bool enablePaneHandleDragging{ true };
        bool showSnapLayoutOverlay{ true };
        bool enableCornerZones{ true };
        AfterSplitFocus afterSplit{ AfterSplitFocus::FocusNewPane };
        Actions::SplitProfilePolicy splitProfile{ Actions::SplitProfilePolicy::DefaultProfile };

        void Normalize() noexcept;
        std::vector<std::string> Validate() const;
        bool ShouldShow(size_t paneCount) const noexcept;
        bool HasKeyboardAlternative(bool commandPaletteAvailable, bool keyboardDockingAvailable) const noexcept;
    };
}
