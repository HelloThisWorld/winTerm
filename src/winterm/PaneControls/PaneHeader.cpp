// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PaneHeader.h"

#include <utility>

using namespace winTerm::PaneControls;

PaneHeader::PaneHeader(
    PaneHeaderSettings settings,
    PaneIcon::OpenMenuCallback openMenu,
    PaneIcon::FocusCallback focus) :
    _settings{ std::move(settings) },
    _icon{ std::move(openMenu), std::move(focus) }
{
    _settings.Normalize();
}

PaneHeaderPresentation PaneHeader::Present(const PaneHeaderState& state) const
{
    return PaneHeaderViewModel::Build(state, _settings);
}

PaneIcon& PaneHeader::Icon() noexcept
{
    return _icon;
}

const PaneHeaderSettings& PaneHeader::Settings() const noexcept
{
    return _settings;
}

void PaneHeader::Settings(PaneHeaderSettings settings)
{
    settings.Normalize();
    _settings = std::move(settings);
}
