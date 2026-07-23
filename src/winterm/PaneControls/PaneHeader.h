// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "PaneIcon.h"
#include "PaneHeaderViewModel.h"

namespace winTerm::PaneControls
{
    class PaneHeader
    {
    public:
        PaneHeader(
            PaneHeaderSettings settings,
            PaneIcon::OpenMenuCallback openMenu = {},
            PaneIcon::FocusCallback focus = {});

        PaneHeaderPresentation Present(const PaneHeaderState& state) const;
        PaneIcon& Icon() noexcept;
        const PaneHeaderSettings& Settings() const noexcept;
        void Settings(PaneHeaderSettings settings);

    private:
        PaneHeaderSettings _settings;
        PaneIcon _icon;
    };
}
