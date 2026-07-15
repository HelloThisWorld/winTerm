// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Model/WorkspaceDescriptor.h"
#include "AtomicFileWriter.h"

#include <filesystem>
#include <mutex>
#include <optional>
#include <vector>

namespace winTerm::Workspaces
{
    enum class WorkspaceLoadStatus
    {
        Loaded,
        LoadedFromBackup,
        Missing,
        Corrupted,
        NewerSchema,
    };

    struct WorkspaceLoadResult
    {
        WorkspaceLoadStatus status{ WorkspaceLoadStatus::Missing };
        std::optional<WorkspaceDescriptor> workspace;
        std::filesystem::path sourcePath;
        std::string message;
    };

    struct WorkspaceStorePaths
    {
        std::filesystem::path root;
        std::filesystem::path lastSession;
        std::filesystem::path lastSessionBackup;
        std::filesystem::path snapshots;
        std::filesystem::path named;
        std::filesystem::path imported;
        std::filesystem::path quarantine;
        std::filesystem::path index;
        std::filesystem::path runningMarker;
        std::filesystem::path cleanShutdownMarker;
    };

    class WorkspaceStore final
    {
    public:
        explicit WorkspaceStore(std::filesystem::path applicationStateRoot);

        const WorkspaceStorePaths& Paths() const noexcept;
        AtomicWriteResult SaveLastSession(const WorkspaceDescriptor& workspace, uint64_t generation);
        WorkspaceLoadResult LoadLastSession(bool quarantineCorrupted = true) const;

        AtomicWriteResult SaveSnapshot(const WorkspaceDescriptor& workspace, uint64_t generation, size_t retentionCount = 3);
        std::vector<std::filesystem::path> SnapshotPaths() const;
        WorkspaceLoadResult LoadLatestValidSnapshot() const;
        void ClearSnapshots();

        AtomicWriteResult SaveNamed(const WorkspaceDescriptor& workspace);
        WorkspaceLoadResult LoadNamed(std::string_view workspaceId) const;
        std::vector<WorkspaceDescriptor> LoadAllNamed() const;
        bool DeleteNamed(std::string_view workspaceId);
        void RebuildIndex();

        void MarkSessionRunning(std::string_view timestamp);
        void MarkCleanShutdown(std::string_view timestamp);
        bool WasLastShutdownClean() const;
        void ClearLastSession();

        uint64_t LatestWrittenGeneration() const noexcept;

    private:
        WorkspaceLoadResult _load(const std::filesystem::path& path, const std::optional<std::filesystem::path>& backup, bool quarantineCorrupted) const;
        std::filesystem::path _namedPath(std::string_view workspaceId) const;
        std::filesystem::path _quarantine(const std::filesystem::path& path, std::string_view reason) const;
        static bool _isSafeId(std::string_view value) noexcept;
        static bool _isValidContent(std::string_view content) noexcept;

        WorkspaceStorePaths _paths;
        mutable std::mutex _writeMutex;
        uint64_t _latestWrittenGeneration{};
    };
}
