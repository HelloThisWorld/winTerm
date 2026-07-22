// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "PaneCommandMenu.h"

#include <functional>

namespace winTerm::PaneControls
{
    enum class PanePointerRegion
    {
        DragGrip,
        HeaderBody,
        OverflowButton,
        TerminalContent,
        Scrollbar,
    };

    class PaneHandle
    {
    public:
        using OpenMenuCallback = std::function<void(PaneMenuInvocation)>;
        using FocusCallback = std::function<void()>;

        explicit PaneHandle(OpenMenuCallback openMenu = {}, FocusCallback focus = {});

        bool CanStartDrag(PanePointerRegion region, bool draggingEnabled) const noexcept;
        bool HandleRightClick(PanePointerRegion region) const;
        bool HandlePrimaryClick(PanePointerRegion region) const;

    private:
        OpenMenuCallback _openMenu;
        FocusCallback _focus;
    };
}
