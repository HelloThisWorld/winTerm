// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceRuntime.h"

using namespace winTerm::Workspaces;

uint64_t WorkspaceDirtyTracker::MarkDirty() noexcept
{
    return _generation.fetch_add(1, std::memory_order_acq_rel) + 1;
}

uint64_t WorkspaceDirtyTracker::CurrentGeneration() const noexcept
{
    return _generation.load(std::memory_order_acquire);
}

uint64_t WorkspaceDirtyTracker::CommittedGeneration() const noexcept
{
    return _committedGeneration.load(std::memory_order_acquire);
}

bool WorkspaceDirtyTracker::IsDirty() const noexcept
{
    return CurrentGeneration() != CommittedGeneration();
}

bool WorkspaceDirtyTracker::TryCommit(const uint64_t generation) noexcept
{
    if (generation != CurrentGeneration())
    {
        return false;
    }
    auto committed = _committedGeneration.load(std::memory_order_acquire);
    while (committed < generation)
    {
        if (_committedGeneration.compare_exchange_weak(committed, generation, std::memory_order_acq_rel))
        {
            return true;
        }
    }
    return committed == generation;
}

WorkspaceAutosaveScheduler::WorkspaceAutosaveScheduler(WorkspaceAutosavePolicy policy) :
    _policy{ policy },
    _lastSnapshot{ std::chrono::steady_clock::now() }
{
}

uint64_t WorkspaceAutosaveScheduler::MarkDirty(const WorkspaceMutation mutation, const std::chrono::steady_clock::time_point now)
{
    const auto generation = _dirty.MarkDirty();
    std::scoped_lock lock{ _mutex };
    _dueAt = now + _delayFor(mutation);
    return generation;
}

bool WorkspaceAutosaveScheduler::IsSaveDue(const std::chrono::steady_clock::time_point now) const
{
    std::scoped_lock lock{ _mutex };
    return _dirty.IsDirty() && _dueAt && now >= *_dueAt;
}

std::optional<uint64_t> WorkspaceAutosaveScheduler::BeginSave(const std::chrono::steady_clock::time_point now) const
{
    if (!IsSaveDue(now))
    {
        return std::nullopt;
    }
    return _dirty.CurrentGeneration();
}

bool WorkspaceAutosaveScheduler::CompleteSave(const uint64_t generation)
{
    std::scoped_lock lock{ _mutex };
    if (_dirty.TryCommit(generation))
    {
        _dueAt.reset();
        return true;
    }
    return false;
}

bool WorkspaceAutosaveScheduler::IsSnapshotDue(const std::chrono::steady_clock::time_point now) const
{
    std::scoped_lock lock{ _mutex };
    return _dirty.IsDirty() && now - _lastSnapshot >= _policy.crashSnapshotInterval;
}

void WorkspaceAutosaveScheduler::SnapshotCompleted(const std::chrono::steady_clock::time_point now)
{
    std::scoped_lock lock{ _mutex };
    _lastSnapshot = now;
}

const WorkspaceDirtyTracker& WorkspaceAutosaveScheduler::DirtyTracker() const noexcept
{
    return _dirty;
}

std::chrono::milliseconds WorkspaceAutosaveScheduler::_delayFor(const WorkspaceMutation mutation) const noexcept
{
    switch (mutation)
    {
    case WorkspaceMutation::WindowBounds:
        return _policy.windowBoundsDelay;
    case WorkspaceMutation::CurrentDirectory:
        return _policy.currentDirectoryDelay;
    case WorkspaceMutation::FinalSave:
        return std::chrono::milliseconds::zero();
    default:
        return _policy.structuralDelay;
    }
}

WorkspaceRecoveryState WorkspaceRecoveryCoordinator::Inspect(const WorkspaceStore& store)
{
    WorkspaceRecoveryState state;
    state.previousShutdownWasClean = store.WasLastShutdownClean();
    state.lastSession = store.LoadLastSession(false);
    state.latestSnapshot = store.LoadLatestValidSnapshot();
    if (!state.previousShutdownWasClean && state.latestSnapshot.workspace)
    {
        state.recommendation = RecoveryRecommendation::OfferLatestSnapshot;
    }
    else if (state.lastSession.workspace)
    {
        state.recommendation = RecoveryRecommendation::OfferLastSession;
    }
    else
    {
        state.recommendation = RecoveryRecommendation::OpenNewSession;
    }
    return state;
}
