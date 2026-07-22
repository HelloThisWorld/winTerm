// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../../PaneControls/PaneHandle.h"
#include "../Layout/LayoutTransformer.h"
#include "../Overlay/PaneSnapLayoutOverlay.h"
#include "DockDragSession.h"

#include <chrono>
#include <optional>
#include <string>

namespace winTerm::Docking
{
    struct PaneHandleDragContext
    {
        std::string processInstanceId;
        DockSource source;
        LayoutNodePtr sourcePaneLayout;
        LayoutNodePtr sourceTabLayout;
        DragPoint pressedPoint;
        DragThresholdSettings threshold;
        bool draggingEnabled{ true };
    };

    struct PaneHandleDragPreviewRequest
    {
        DockTarget target;
        DockZone zone{ DockZone::Center };
        LayoutNodePtr targetLayout;
        std::string emptySlotId;
        DockingCapabilities capabilities;
        bool sameProcess{ true };
        LayoutRect targetBounds;
        LayoutGeometrySettings geometrySettings;
    };

    struct PaneHandleDragPreview
    {
        DockingPlan plan;
        DockPreviewModel preview;
        std::vector<DockZonePresentation> overlay;

        bool Succeeded() const noexcept;
    };

    class PaneHandleDragSource
    {
    public:
        explicit PaneHandleDragSource(DragPayloadRegistry& registry);

        bool PointerPressed(
            PaneControls::PanePointerRegion region,
            PaneHandleDragContext context);
        bool PointerMoved(
            DragPoint current,
            std::chrono::milliseconds held);
        bool PointerReleased();
        PaneHandleDragPreview Preview(const PaneHandleDragPreviewRequest& request);
        bool RequestDrop();
        bool BeginCommit();
        bool Complete();
        bool Fail(std::string reason);
        bool BeginRollback();
        bool CompleteRollback(bool restored);
        bool Cancel(DragCancellationReason reason);
        bool Reset();

        DockDragState State() const noexcept;
        DragCancellationReason CancellationReason() const noexcept;
        std::string_view FailureReason() const noexcept;
        std::string_view Token() const noexcept;

    private:
        void _invalidateToken() noexcept;

        DragPayloadRegistry& _registry;
        DockDragStateMachine _state;
        std::optional<PaneHandleDragContext> _context;
        std::string _token;
        std::optional<DockingPlan> _lastPlan;
    };
}
