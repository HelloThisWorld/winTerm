// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../../PaneControls/PaneHeaderSettings.h"
#include "../../PaneResize/PaneResizeModel.h"

#include <cstddef>
#include <string>
#include <vector>

namespace winTerm::Settings
{
    struct DockingSettingsModel
    {
        bool enableTabDragging{ true };
        bool allowTabTearOut{ true };
        double defaultSplitRatio{ 0.5 };
        bool enableLayoutUndo{ true };
        size_t layoutHistorySize{ 20 };
        bool includePaneResizeInHistory{ true };
        PaneResize::PaneResizeSettings paneResizing;
        PaneControls::PaneHeaderSettings paneControls;

        void Normalize() noexcept;
        std::vector<std::string> Validate() const;
    };
}
