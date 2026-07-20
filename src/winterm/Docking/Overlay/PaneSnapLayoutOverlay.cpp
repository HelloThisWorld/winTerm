// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PaneSnapLayoutOverlay.h"

using namespace winTerm::Docking;

std::vector<DockZonePresentation> PaneSnapLayoutOverlay::Build(
    const LayoutRect contentBounds,
    const DockSource& source,
    const DockTarget& target,
    DockingCapabilities capabilities,
    const bool sameProcess,
    const std::optional<DockZone> selected,
    const bool cornerZonesEnabled,
    const DockingOverlaySettings& settings)
{
    capabilities.canDockCorners = capabilities.canDockCorners && cornerZonesEnabled;
    capabilities.canUseEmptySlots = capabilities.canUseEmptySlots && cornerZonesEnabled;
    return DockingOverlayModel::Build(
        contentBounds,
        source,
        target,
        capabilities,
        sameProcess,
        selected,
        settings);
}
