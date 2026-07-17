// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Layout/LayoutGeometry.h"
#include "../Model/DockingModel.h"

#include <optional>
#include <string>
#include <vector>

namespace winTerm::Docking
{
    struct DockZonePresentation
    {
        DockZone zone{ DockZone::Center };
        LayoutRect hitBounds;
        std::string label;
        std::string automationName;
        std::string automationRole{ "Button" };
        std::string icon;
        bool enabled{ true };
        bool selected{ false };
        bool visible{ true };
        std::string disabledReason;
    };

    struct DockingOverlaySettings
    {
        bool showLabels{ true };
        bool highContrast{ false };
        double dpiScale{ 1.0 };
        double tileSize{ 64.0 };
        double tileGap{ 8.0 };
        double minimumCornerTargetWidth{ 480.0 };
        double minimumCornerTargetHeight{ 300.0 };
    };

    class DockingOverlayModel
    {
    public:
        static std::vector<DockZonePresentation> Build(
            LayoutRect contentBounds,
            const DockSource& source,
            const DockTarget& target,
            const DockingCapabilities& capabilities,
            bool sameProcess,
            std::optional<DockZone> selected,
            const DockingOverlaySettings& settings = {});
    };

    enum class DockPreviewRegionRole
    {
        Source,
        Target,
        EmptySlot,
    };

    struct DockPreviewRegion
    {
        std::string nodeId;
        LayoutRect bounds;
        DockPreviewRegionRole role{ DockPreviewRegionRole::Target };
        std::string automationName;
    };

    struct DockPreviewModel
    {
        bool valid{ false };
        std::string invalidReason;
        std::string automationSummary;
        std::vector<DockPreviewRegion> regions;
    };

    class DockPreview
    {
    public:
        static DockPreviewModel Build(
            const DockingPlan& plan,
            LayoutRect targetBounds,
            const LayoutGeometrySettings& settings = {});
    };
}
