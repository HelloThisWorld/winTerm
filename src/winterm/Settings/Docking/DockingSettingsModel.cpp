// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "DockingSettingsModel.h"

#include <algorithm>
#include <cmath>

using namespace winTerm::Settings;

void DockingSettingsModel::Normalize() noexcept
{
    if (!std::isfinite(defaultSplitRatio))
    {
        defaultSplitRatio = 0.5;
    }
    defaultSplitRatio = std::clamp(defaultSplitRatio, 0.2, 0.8);
    layoutHistorySize = std::clamp<size_t>(layoutHistorySize, 1, 100);
    paneResizing.Normalize();
    paneControls.Normalize();
}

std::vector<std::string> DockingSettingsModel::Validate() const
{
    std::vector<std::string> errors;
    if (!std::isfinite(defaultSplitRatio) || defaultSplitRatio < 0.2 || defaultSplitRatio > 0.8)
    {
        errors.emplace_back("Default split ratio must be between 0.2 and 0.8.");
    }
    if (layoutHistorySize < 1 || layoutHistorySize > 100)
    {
        errors.emplace_back("Layout history size must be between 1 and 100.");
    }
    const auto paneResizeErrors = paneResizing.Validate();
    errors.insert(errors.end(), paneResizeErrors.begin(), paneResizeErrors.end());
    const auto paneControlErrors = paneControls.Validate();
    errors.insert(errors.end(), paneControlErrors.begin(), paneControlErrors.end());
    if (!paneControls.HasKeyboardAlternative(true))
    {
        errors.emplace_back("Hidden pane headers require a keyboard or Command Palette alternative.");
    }
    return errors;
}
