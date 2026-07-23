// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PaneIcon.h"

#include <utility>

using namespace winTerm::PaneControls;

PaneIcon::PaneIcon(OpenMenuCallback openMenu, FocusCallback focus) :
    _openMenu{ std::move(openMenu) },
    _focus{ std::move(focus) }
{
}

bool PaneIcon::HandleRightClick(const PanePointerRegion region) const
{
    if (region != PanePointerRegion::PaneIcon && region != PanePointerRegion::HeaderBody)
    {
        return false;
    }
    if (_openMenu)
    {
        _openMenu(PaneMenuInvocation::IconRightClick);
    }
    return true;
}

bool PaneIcon::HandlePrimaryClick(const PanePointerRegion region) const
{
    if (region == PanePointerRegion::PaneIcon || region == PanePointerRegion::HeaderBody)
    {
        if (_focus)
        {
            _focus();
        }
        return true;
    }
    if (region != PanePointerRegion::OverflowButton)
    {
        return false;
    }
    if (_openMenu)
    {
        _openMenu(PaneMenuInvocation::OverflowButton);
    }
    return true;
}
