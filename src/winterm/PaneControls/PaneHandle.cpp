// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PaneHandle.h"

#include <utility>

using namespace winTerm::PaneControls;

PaneHandle::PaneHandle(OpenMenuCallback openMenu) :
    _openMenu{ std::move(openMenu) }
{
}

bool PaneHandle::CanStartDrag(
    const PanePointerRegion region,
    const bool draggingEnabled) const noexcept
{
    return draggingEnabled && region == PanePointerRegion::DragGrip;
}

bool PaneHandle::HandleRightClick(const PanePointerRegion region) const
{
    if (region != PanePointerRegion::DragGrip && region != PanePointerRegion::HeaderBody)
    {
        return false;
    }
    if (_openMenu)
    {
        _openMenu(PaneMenuInvocation::HandleRightClick);
    }
    return true;
}

bool PaneHandle::HandlePrimaryClick(const PanePointerRegion region) const
{
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
