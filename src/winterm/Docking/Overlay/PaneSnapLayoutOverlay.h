// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "DockingOverlayModel.h"

namespace winTerm::Docking
{
    struct PaneSnapLayoutOverlay
    {
        static std::vector<DockZonePresentation> Build(
            LayoutRect contentBounds,
            const DockSource& source,
            const DockTarget& target,
            DockingCapabilities capabilities,
            bool sameProcess,
            std::optional<DockZone> selected,
            bool cornerZonesEnabled = true,
            const DockingOverlaySettings& settings = {});
    };
}
