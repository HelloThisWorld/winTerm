// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PaneHeaderSettings.h"

#include <algorithm>
#include <cmath>

using namespace winTerm::PaneControls;

void PaneHeaderSettings::Normalize() noexcept
{
    if (!std::isfinite(height))
    {
        height = 26.0;
    }
    height = std::clamp(height, 24.0, 30.0);
}

std::vector<std::string> PaneHeaderSettings::Validate() const
{
    std::vector<std::string> errors;
    if (!std::isfinite(height) || height < 24.0 || height > 30.0)
    {
        errors.emplace_back("Pane header height must be between 24 and 30 logical pixels.");
    }
    return errors;
}

bool PaneHeaderSettings::ShouldShow(const size_t paneCount) const noexcept
{
    switch (visibility)
    {
    case PaneHeaderVisibility::Always:
        return true;
    case PaneHeaderVisibility::Never:
        return false;
    case PaneHeaderVisibility::Automatic:
    default:
        return paneCount >= 2;
    }
}

bool PaneHeaderSettings::HasKeyboardAlternative(
    const bool commandPaletteAvailable,
    const bool keyboardDockingAvailable) const noexcept
{
    return visibility != PaneHeaderVisibility::Never ||
           commandPaletteAvailable ||
           keyboardDockingAvailable;
}
