// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PaneHeader.h"

#include <utility>

using namespace winTerm::PaneControls;

PaneHeader::PaneHeader(
    PaneHeaderSettings settings,
    PaneHandle::OpenMenuCallback openMenu) :
    _settings{ std::move(settings) },
    _handle{ std::move(openMenu) }
{
    _settings.Normalize();
}

PaneHeaderPresentation PaneHeader::Present(const PaneHeaderState& state) const
{
    return PaneHeaderViewModel::Build(state, _settings);
}

PaneHandle& PaneHeader::Handle() noexcept
{
    return _handle;
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
