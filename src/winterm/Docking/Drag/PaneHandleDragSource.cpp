// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PaneHandleDragSource.h"

#include "../Layout/LayoutTree.h"

#include <utility>

using namespace winTerm::Docking;
using namespace winTerm::PaneControls;

bool PaneHandleDragPreview::Succeeded() const noexcept
{
    return plan.status == DockingStatus::Ready && preview.valid;
}

PaneHandleDragSource::PaneHandleDragSource(DragPayloadRegistry& registry) :
    _registry{ registry }
{
}

bool PaneHandleDragSource::PointerPressed(
    const PanePointerRegion region,
    PaneHandleDragContext context)
{
    if (region != PanePointerRegion::DragGrip ||
        !context.draggingEnabled ||
        context.processInstanceId.empty() ||
        !context.source.paneId ||
        !context.sourcePaneLayout ||
        !context.sourceTabLayout ||
        _state.State() != DockDragState::Idle)
    {
        return false;
    }
    if (!_state.Transition(DockDragState::PointerPressed) ||
        !_state.Transition(DockDragState::DragPending))
    {
        return false;
    }
    _context = std::move(context);
    return true;
}

bool PaneHandleDragSource::PointerMoved(
    const DragPoint current,
    const std::chrono::milliseconds held)
{
    if (!_context || _state.State() != DockDragState::DragPending)
    {
        return false;
    }
    if (!DragThreshold::Exceeded(
            _context->pressedPoint,
            current,
            held,
            _context->threshold))
    {
        return false;
    }
    _token = _registry.Register(
        _context->processInstanceId,
        _context->source,
        DragCapability::Pane);
    if (_token.empty())
    {
        _state.Cancel(DragCancellationReason::TokenExpired);
        return false;
    }
    return _state.Transition(DockDragState::Dragging);
}

PaneHandleDragPreview PaneHandleDragSource::Preview(
    const PaneHandleDragPreviewRequest& request)
{
    PaneHandleDragPreview result;
    if (!_context ||
        (_state.State() != DockDragState::Dragging &&
         _state.State() != DockDragState::TargetAcquired))
    {
        result.plan.invalidReason = "A pane handle drag is not active.";
        result.preview.invalidReason = result.plan.invalidReason;
        return result;
    }

    result.plan = LayoutTransformer::BuildProposedLayout(
        _context->source,
        request.target,
        request.zone,
        _context->sourcePaneLayout,
        request.targetLayout,
        request.emptySlotId,
        request.capabilities,
        request.sameProcess);
    result.overlay = PaneSnapLayoutOverlay::Build(
        request.targetBounds,
        _context->source,
        request.target,
        request.capabilities,
        request.sameProcess,
        request.zone);
    result.preview = DockPreview::Build(
        result.plan,
        request.targetBounds,
        request.geometrySettings);

    if (result.plan.status == DockingStatus::Ready && result.preview.valid)
    {
        _lastPlan = result.plan;
        if (_state.State() == DockDragState::Dragging)
        {
            _state.Transition(DockDragState::TargetAcquired);
        }
    }
    else if (_state.State() == DockDragState::TargetAcquired)
    {
        _state.Transition(DockDragState::Dragging);
        _lastPlan.reset();
    }
    return result;
}

bool PaneHandleDragSource::RequestDrop()
{
    return _lastPlan &&
           _lastPlan->status == DockingStatus::Ready &&
           _state.Transition(DockDragState::DropPending);
}

bool PaneHandleDragSource::BeginCommit()
{
    return _state.Transition(DockDragState::Committing);
}

bool PaneHandleDragSource::Complete()
{
    if (!_state.Transition(DockDragState::Completed))
    {
        return false;
    }
    _invalidateToken();
    return true;
}

bool PaneHandleDragSource::Cancel(const DragCancellationReason reason)
{
    const auto cancelled = _state.Cancel(reason);
    if (cancelled)
    {
        _invalidateToken();
    }
    return cancelled;
}

bool PaneHandleDragSource::Reset()
{
    if (!_state.Reset())
    {
        return false;
    }
    _context.reset();
    _lastPlan.reset();
    _token.clear();
    return true;
}

DockDragState PaneHandleDragSource::State() const noexcept
{
    return _state.State();
}

std::string_view PaneHandleDragSource::Token() const noexcept
{
    return _token;
}

void PaneHandleDragSource::_invalidateToken() noexcept
{
    if (!_token.empty())
    {
        _registry.Invalidate(_token);
        _token.clear();
    }
}
