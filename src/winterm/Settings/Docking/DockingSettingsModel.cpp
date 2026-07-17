// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "DockingSettingsModel.h"

#include <algorithm>
#include <cmath>

using namespace winTerm::Settings;

void DockingSettingsModel::Normalize() noexcept
{
    if (!std::isfinite(cornerWidthRatio))
    {
        cornerWidthRatio = 0.35;
    }
    if (!std::isfinite(defaultSplitRatio))
    {
        defaultSplitRatio = 0.5;
    }
    cornerWidthRatio = std::clamp(cornerWidthRatio, 0.2, 0.45);
    defaultSplitRatio = std::clamp(defaultSplitRatio, 0.2, 0.8);
    layoutHistorySize = std::clamp<size_t>(layoutHistorySize, 1, 100);
}

std::vector<std::string> DockingSettingsModel::Validate() const
{
    std::vector<std::string> errors;
    if (!std::isfinite(cornerWidthRatio) || cornerWidthRatio < 0.2 || cornerWidthRatio > 0.45)
    {
        errors.emplace_back("Corner width ratio must be between 0.2 and 0.45.");
    }
    if (!std::isfinite(defaultSplitRatio) || defaultSplitRatio < 0.2 || defaultSplitRatio > 0.8)
    {
        errors.emplace_back("Default split ratio must be between 0.2 and 0.8.");
    }
    if (layoutHistorySize < 1 || layoutHistorySize > 100)
    {
        errors.emplace_back("Layout history size must be between 1 and 100.");
    }
    if (enableCornerDocking && !enableEmptyLayoutSlots)
    {
        errors.emplace_back("Corner docking requires empty layout slots.");
    }
    return errors;
}

bool DockingSettingsModel::RuntimeDockingAvailable(const DockingRuntimeReadiness& readiness) const noexcept
{
    return enableRuntimeDocking &&
           readiness.sameProcessWindowHosting &&
           readiness.liveViewReattachment &&
           readiness.transactionRollbackVerified &&
           readiness.runtimeUiVerified;
}

bool DockingSettingsModel::CrossWindowTransferAvailable(
    const DockingRuntimeReadiness& readiness,
    const bool sameProcess) const noexcept
{
    return allowCrossWindowTransfer &&
           sameProcess &&
           RuntimeDockingAvailable(readiness);
}
