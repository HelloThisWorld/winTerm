// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "PaneHandle.h"
#include "PaneHeaderViewModel.h"

namespace winTerm::PaneControls
{
    class PaneHeader
    {
    public:
        PaneHeader(PaneHeaderSettings settings, PaneHandle::OpenMenuCallback openMenu = {});

        PaneHeaderPresentation Present(const PaneHeaderState& state) const;
        PaneHandle& Handle() noexcept;
        const PaneHeaderSettings& Settings() const noexcept;
        void Settings(PaneHeaderSettings settings);

    private:
        PaneHeaderSettings _settings;
        PaneHandle _handle;
    };
}
