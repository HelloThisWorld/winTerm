// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceStore.h"
#include "WorkspaceSerializer.h"
#include "WorkspaceMigration.h"
#include "WorkspaceValidator.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <json/json.h>
#include <sstream>
#include <stdexcept>

using namespace winTerm::Workspaces;

namespace
{
    std::string SnapshotName(const uint64_t generation)
    {
        std::ostringstream stream;
        stream << "snapshot-" << std::setfill('0') << std::setw(20) << generation << ".json";
        return stream.str();
    }

    std::string MarkerContent(const std::string_view timestamp, const bool clean)
    {
        Json::Value json{ Json::objectValue };
        json["clean"] = clean;
        json["timestamp"] = std::string{ timestamp };
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        builder["commentStyle"] = "None";
        return Json::writeString(builder, json) + "\n";
    }
}

WorkspaceStore::WorkspaceStore(std::filesystem::path applicationStateRoot)
{
    _paths.root = std::move(applicationStateRoot) / "workspaces";
    _paths.lastSession = _paths.root / "last-session.json";
    _paths.lastSessionBackup = _paths.root / "last-session.backup.json";
    _paths.snapshots = _paths.root / "snapshots";
    _paths.named = _paths.root / "named";
    _paths.imported = _paths.root / "imported";
    _paths.quarantine = _paths.root / "quarantine";
    _paths.index = _paths.root / "index.json";
    _paths.runningMarker = _paths.root / "session-running.json";
    _paths.cleanShutdownMarker = _paths.root / "clean-shutdown.json";
    std::filesystem::create_directories(_paths.snapshots);
    std::filesystem::create_directories(_paths.named);
    std::filesystem::create_directories(_paths.imported);
    std::filesystem::create_directories(_paths.quarantine);
}

const WorkspaceStorePaths& WorkspaceStore::Paths() const noexcept
{
    return _paths;
}

AtomicWriteResult WorkspaceStore::SaveLastSession(const WorkspaceDescriptor& workspace, const uint64_t generation)
{
    std::scoped_lock lock{ _writeMutex };
    if (generation < _latestWrittenGeneration)
    {
        return { false, false, "An older workspace save generation cannot replace a newer generation." };
    }
    const auto content = WorkspaceSerializer::Serialize(workspace);
    auto result = AtomicFileWriter::Write(_paths.lastSession, content, _paths.lastSessionBackup, _isValidContent);
    if (result.succeeded)
    {
        _latestWrittenGeneration = generation;
    }
    return result;
}

WorkspaceLoadResult WorkspaceStore::LoadLastSession(const bool quarantineCorrupted) const
{
    return _load(_paths.lastSession, _paths.lastSessionBackup, quarantineCorrupted);
}

AtomicWriteResult WorkspaceStore::SaveSnapshot(const WorkspaceDescriptor& workspace, const uint64_t generation, const size_t retentionCount)
{
    std::scoped_lock lock{ _writeMutex };
    if (generation < _latestWrittenGeneration)
    {
        return { false, false, "An older workspace snapshot generation cannot replace a newer generation." };
    }
    const auto path = _paths.snapshots / SnapshotName(generation);
    auto result = AtomicFileWriter::Write(path, WorkspaceSerializer::Serialize(workspace));
    if (!result.succeeded)
    {
        return result;
    }
    _latestWrittenGeneration = generation;

    auto snapshots = SnapshotPaths();
    const auto keep = std::max<size_t>(1, retentionCount);
    while (snapshots.size() > keep)
    {
        std::error_code error;
        std::filesystem::remove(snapshots.back(), error);
        snapshots.pop_back();
    }
    return result;
}

std::vector<std::filesystem::path> WorkspaceStore::SnapshotPaths() const
{
    std::vector<std::filesystem::path> result;
    std::error_code error;
    if (!std::filesystem::is_directory(_paths.snapshots, error))
    {
        return result;
    }
    for (const auto& entry : std::filesystem::directory_iterator{ _paths.snapshots, error })
    {
        const auto filename = entry.path().filename().string();
        if (entry.is_regular_file() && entry.path().extension() == ".json" && filename.rfind("snapshot-", 0) == 0)
        {
            result.emplace_back(entry.path());
        }
    }
    std::sort(result.begin(), result.end(), std::greater<>());
    return result;
}

WorkspaceLoadResult WorkspaceStore::LoadLatestValidSnapshot() const
{
    WorkspaceLoadResult lastFailure;
    for (const auto& snapshot : SnapshotPaths())
    {
        auto result = _load(snapshot, std::nullopt, false);
        if (result.status == WorkspaceLoadStatus::Loaded)
        {
            return result;
        }
        lastFailure = std::move(result);
    }
    return lastFailure.status == WorkspaceLoadStatus::Missing ? WorkspaceLoadResult{} : lastFailure;
}

void WorkspaceStore::ClearSnapshots()
{
    for (const auto& snapshot : SnapshotPaths())
    {
        std::error_code error;
        std::filesystem::remove(snapshot, error);
    }
}

AtomicWriteResult WorkspaceStore::SaveNamed(const WorkspaceDescriptor& workspace)
{
    if (!_isSafeId(workspace.id))
    {
        return { false, false, "The named workspace identifier is not safe for local storage." };
    }
    auto result = AtomicFileWriter::Write(_namedPath(workspace.id), WorkspaceSerializer::Serialize(workspace));
    if (result.succeeded)
    {
        RebuildIndex();
    }
    return result;
}

WorkspaceLoadResult WorkspaceStore::LoadNamed(const std::string_view workspaceId) const
{
    if (!_isSafeId(workspaceId))
    {
        return { WorkspaceLoadStatus::Corrupted, std::nullopt, {}, "The named workspace identifier is invalid." };
    }
    return _load(_namedPath(workspaceId), std::nullopt, true);
}

std::vector<WorkspaceDescriptor> WorkspaceStore::LoadAllNamed() const
{
    std::vector<WorkspaceDescriptor> result;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator{ _paths.named, error })
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
        {
            continue;
        }
        const auto loaded = _load(entry.path(), std::nullopt, false);
        if (loaded.workspace)
        {
            result.emplace_back(*loaded.workspace);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.updatedAt > right.updatedAt;
    });
    return result;
}

bool WorkspaceStore::DeleteNamed(const std::string_view workspaceId)
{
    if (!_isSafeId(workspaceId))
    {
        return false;
    }
    std::error_code error;
    const auto removed = std::filesystem::remove(_namedPath(workspaceId), error);
    if (removed && !error)
    {
        RebuildIndex();
        return true;
    }
    return false;
}

void WorkspaceStore::RebuildIndex()
{
    Json::Value index{ Json::objectValue };
    index["schemaVersion"] = WorkspaceSchemaVersion;
    Json::Value entries{ Json::arrayValue };
    for (const auto& workspace : LoadAllNamed())
    {
        Json::Value entry{ Json::objectValue };
        entry["id"] = workspace.id;
        entry["name"] = workspace.name;
        entry["path"] = _namedPath(workspace.id).filename().string();
        entry["updatedAt"] = workspace.updatedAt;
        entry["isDefault"] = workspace.isDefault;
        if (workspace.lastOpenedAt) entry["lastOpenedAt"] = *workspace.lastOpenedAt;
        entry["health"] = "valid";
        entries.append(std::move(entry));
    }
    index["workspaces"] = std::move(entries);
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    builder["commentStyle"] = "None";
    const auto result = AtomicFileWriter::Write(_paths.index, Json::writeString(builder, index) + "\n");
    if (!result.succeeded)
    {
        throw std::runtime_error(result.error);
    }
}

void WorkspaceStore::MarkSessionRunning(const std::string_view timestamp)
{
    const auto result = AtomicFileWriter::Write(_paths.runningMarker, MarkerContent(timestamp, false));
    if (!result.succeeded)
    {
        throw std::runtime_error(result.error);
    }
    std::error_code error;
    std::filesystem::remove(_paths.cleanShutdownMarker, error);
}

void WorkspaceStore::MarkCleanShutdown(const std::string_view timestamp)
{
    const auto result = AtomicFileWriter::Write(_paths.cleanShutdownMarker, MarkerContent(timestamp, true));
    if (!result.succeeded)
    {
        throw std::runtime_error(result.error);
    }
    std::error_code error;
    std::filesystem::remove(_paths.runningMarker, error);
}

bool WorkspaceStore::WasLastShutdownClean() const
{
    return std::filesystem::is_regular_file(_paths.cleanShutdownMarker) && !std::filesystem::is_regular_file(_paths.runningMarker);
}

void WorkspaceStore::ClearLastSession()
{
    std::error_code error;
    std::filesystem::remove(_paths.lastSession, error);
    error.clear();
    std::filesystem::remove(_paths.lastSessionBackup, error);
}

uint64_t WorkspaceStore::LatestWrittenGeneration() const noexcept
{
    std::scoped_lock lock{ _writeMutex };
    return _latestWrittenGeneration;
}

WorkspaceLoadResult WorkspaceStore::_load(
    const std::filesystem::path& path,
    const std::optional<std::filesystem::path>& backup,
    const bool quarantineCorrupted) const
{
    if (!std::filesystem::is_regular_file(path))
    {
        return { WorkspaceLoadStatus::Missing, std::nullopt, path, "The workspace file does not exist." };
    }
    try
    {
        const auto content = WorkspaceSerializer::ReadFile(path);
        auto json = WorkspaceSerializer::ParseJsonDocument(content);
        if (json["schemaVersion"].isUInt() && json["schemaVersion"].asUInt() > WorkspaceSchemaVersion)
        {
            return { WorkspaceLoadStatus::NewerSchema, std::nullopt, path, "This workspace was created by a newer version of winTerm." };
        }
        if (json["schemaVersion"].isUInt() && json["schemaVersion"].asUInt() < WorkspaceSchemaVersion)
        {
            const auto sourceVersion = json["schemaVersion"].asUInt();
            const auto migrationBackup = std::filesystem::path{ path.string() + ".schema-v" + std::to_string(sourceVersion) + ".backup.json" };
            if (!std::filesystem::is_regular_file(migrationBackup))
            {
                const auto backupResult = AtomicFileWriter::Write(migrationBackup, content);
                if (!backupResult.succeeded)
                {
                    throw std::runtime_error("The workspace could not be backed up before schema migration.");
                }
            }
            json = WorkspaceMigration::Migrate(json).document;
        }
        return { WorkspaceLoadStatus::Loaded, WorkspaceSerializer::FromJson(json), path, {} };
    }
    catch (const std::exception& exception)
    {
        if (backup && std::filesystem::is_regular_file(*backup))
        {
            try
            {
                auto workspace = WorkspaceSerializer::Deserialize(WorkspaceSerializer::ReadFile(*backup));
                if (quarantineCorrupted)
                {
                    _quarantine(path, "invalid workspace data");
                }
                return { WorkspaceLoadStatus::LoadedFromBackup, std::move(workspace), *backup, "The last valid backup was loaded." };
            }
            catch (...)
            {
            }
        }
        if (quarantineCorrupted)
        {
            _quarantine(path, "invalid workspace data");
        }
        return { WorkspaceLoadStatus::Corrupted, std::nullopt, path, exception.what() };
    }
}

std::filesystem::path WorkspaceStore::_namedPath(const std::string_view workspaceId) const
{
    return _paths.named / (std::string{ workspaceId } + ".json");
}

std::filesystem::path WorkspaceStore::_quarantine(const std::filesystem::path& path, const std::string_view reason) const
{
    if (!std::filesystem::is_regular_file(path))
    {
        return {};
    }
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto destination = _paths.quarantine / ("quarantine-" + std::to_string(timestamp) + ".json");
    std::error_code error;
    std::filesystem::rename(path, destination, error);
    if (error)
    {
        return {};
    }
    Json::Value metadata{ Json::objectValue };
    metadata["reason"] = std::string{ reason };
    metadata["sourceType"] = "workspace";
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    const auto metadataPath = destination.parent_path() / (destination.stem().string() + ".metadata.json");
    AtomicFileWriter::Write(metadataPath, Json::writeString(builder, metadata) + "\n");
    return destination;
}

bool WorkspaceStore::_isSafeId(const std::string_view value) noexcept
{
    return !value.empty() && value.size() <= 200 && std::all_of(value.begin(), value.end(), [](const unsigned char character) {
        return std::isalnum(character) || character == '.' || character == '-' || character == '_';
    });
}

bool WorkspaceStore::_isValidContent(const std::string_view content) noexcept
{
    try
    {
        auto document = WorkspaceSerializer::ParseJsonDocument(content);
        if (document["schemaVersion"].isUInt() && document["schemaVersion"].asUInt() < WorkspaceSchemaVersion)
        {
            document = WorkspaceMigration::Migrate(document).document;
        }
        WorkspaceSerializer::FromJson(document);
        return true;
    }
    catch (...)
    {
        return false;
    }
}
