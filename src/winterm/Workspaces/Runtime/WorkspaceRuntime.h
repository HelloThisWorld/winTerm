// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Model/WorkspaceDescriptor.h"
#include "../Persistence/WorkspaceStore.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string_view>

namespace winTerm::Workspaces
{
    enum class WorkspaceMutation
    {
        Structural,
        WindowBounds,
        CurrentDirectory,
        Focus,
        Appearance,
        FinalSave,
    };

    class WorkspaceDirtyTracker final
    {
    public:
        uint64_t MarkDirty() noexcept;
        uint64_t CurrentGeneration() const noexcept;
        uint64_t CommittedGeneration() const noexcept;
        bool IsDirty() const noexcept;
        bool TryCommit(uint64_t generation) noexcept;

    private:
        std::atomic<uint64_t> _generation{};
        std::atomic<uint64_t> _committedGeneration{};
    };

    struct WorkspaceAutosavePolicy
    {
        std::chrono::milliseconds structuralDelay{ 750 };
        std::chrono::milliseconds windowBoundsDelay{ 1000 };
        std::chrono::milliseconds currentDirectoryDelay{ 1000 };
        std::chrono::seconds crashSnapshotInterval{ 30 };
    };

    class WorkspaceAutosaveScheduler final
    {
    public:
        explicit WorkspaceAutosaveScheduler(WorkspaceAutosavePolicy policy = {});

        uint64_t MarkDirty(WorkspaceMutation mutation, std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
        bool IsSaveDue(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const;
        std::optional<uint64_t> BeginSave(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const;
        bool CompleteSave(uint64_t generation);
        bool IsSnapshotDue(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const;
        void SnapshotCompleted(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
        const WorkspaceDirtyTracker& DirtyTracker() const noexcept;

    private:
        std::chrono::milliseconds _delayFor(WorkspaceMutation mutation) const noexcept;

        WorkspaceAutosavePolicy _policy;
        mutable std::mutex _mutex;
        WorkspaceDirtyTracker _dirty;
        std::optional<std::chrono::steady_clock::time_point> _dueAt;
        std::chrono::steady_clock::time_point _lastSnapshot;
    };

    enum class RecoveryRecommendation
    {
        None,
        OfferLatestSnapshot,
        OfferLastSession,
        OpenNewSession,
    };

    struct WorkspaceRecoveryState
    {
        bool previousShutdownWasClean{ true };
        WorkspaceLoadResult lastSession;
        WorkspaceLoadResult latestSnapshot;
        RecoveryRecommendation recommendation{ RecoveryRecommendation::None };
    };

    class WorkspaceRecoveryCoordinator final
    {
    public:
        static WorkspaceRecoveryState Inspect(const WorkspaceStore& store);
    };
}
