// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "PaneCommandMenu.h"

#include <functional>

namespace winTerm::PaneControls
{
    enum class PanePointerRegion
    {
        PaneIcon,
        HeaderBody,
        OverflowButton,
        TerminalContent,
        Scrollbar,
    };

    class PaneIcon
    {
    public:
        using OpenMenuCallback = std::function<void(PaneMenuInvocation)>;
        using FocusCallback = std::function<void()>;

        explicit PaneIcon(OpenMenuCallback openMenu = {}, FocusCallback focus = {});

        bool HandleRightClick(PanePointerRegion region) const;
        bool HandlePrimaryClick(PanePointerRegion region) const;

    private:
        OpenMenuCallback _openMenu;
        FocusCallback _focus;
    };
}
