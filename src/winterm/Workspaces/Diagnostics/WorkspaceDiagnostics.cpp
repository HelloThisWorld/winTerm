// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceDiagnostics.h"

#include <algorithm>
#include <cctype>
#include <sstream>

using namespace winTerm::Workspaces;

namespace
{
    size_t CountJsonFiles(const std::filesystem::path& directory, const std::string_view prefix = {})
    {
        size_t count{};
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator{ directory, error })
        {
            const auto name = entry.path().filename().string();
            if (entry.is_regular_file() && entry.path().extension() == ".json" && (prefix.empty() || name.rfind(prefix, 0) == 0))
            {
                ++count;
            }
        }
        return count;
    }

    std::string HealthText(const WorkspaceLoadStatus status)
    {
        switch (status)
        {
        case WorkspaceLoadStatus::Loaded: return "Valid";
        case WorkspaceLoadStatus::LoadedFromBackup: return "Recovered from backup";
        case WorkspaceLoadStatus::Missing: return "Missing";
        case WorkspaceLoadStatus::NewerSchema: return "Newer schema";
        default: return "Corrupted";
        }
    }
}

WorkspaceDiagnosticsSnapshot WorkspaceDiagnostics::Capture(
    const WorkspaceStore& store,
    const bool autosaveEnabled,
    const bool dirty,
    const uint64_t generation,
    const std::optional<uint64_t> lastRestoreDurationMilliseconds,
    const size_t lastRestoreAdjustments)
{
    WorkspaceDiagnosticsSnapshot result;
    result.storageLocation = store.Paths().root;
    const auto lastSession = store.LoadLastSession(false);
    result.lastSessionHealth = lastSession.status;
    if (lastSession.workspace)
    {
        result.lastSuccessfulSave = lastSession.workspace->updatedAt;
    }
    result.lastShutdownClean = store.WasLastShutdownClean();
    result.snapshotCount = store.SnapshotPaths().size();
    result.namedWorkspaceCount = store.LoadAllNamed().size();
    result.quarantinedFileCount = CountJsonFiles(store.Paths().quarantine, "quarantine-");
    result.lastRestoreDurationMilliseconds = lastRestoreDurationMilliseconds;
    result.lastRestoreAdjustments = lastRestoreAdjustments;
    result.autosaveEnabled = autosaveEnabled;
    result.dirty = dirty;
    result.saveGeneration = generation;
    return result;
}

std::string WorkspaceDiagnostics::CopySafeText(const WorkspaceDiagnosticsSnapshot& diagnostics)
{
    std::ostringstream output;
    output << "winTerm Workspace Diagnostics\n";
    output << "Storage: " << RedactPath(diagnostics.storageLocation.string()) << "\n";
    output << "Schema: " << diagnostics.schemaVersion << "\n";
    output << "Last session: " << HealthText(diagnostics.lastSessionHealth) << "\n";
    output << "Last successful save: " << diagnostics.lastSuccessfulSave << "\n";
    output << "Clean shutdown: " << (diagnostics.lastShutdownClean ? "yes" : "no") << "\n";
    output << "Snapshots: " << diagnostics.snapshotCount << "\n";
    output << "Named workspaces: " << diagnostics.namedWorkspaceCount << "\n";
    output << "Quarantined files: " << diagnostics.quarantinedFileCount << "\n";
    output << "Restore adjustments: " << diagnostics.lastRestoreAdjustments << "\n";
    output << "Autosave: " << (diagnostics.autosaveEnabled ? "enabled" : "disabled") << "\n";
    output << "Dirty: " << (diagnostics.dirty ? "yes" : "no") << "\n";
    output << "Save generation: " << diagnostics.saveGeneration << "\n";
    return output.str();
}

std::string WorkspaceDiagnostics::RedactPath(const std::string_view path)
{
    std::string result{ path };
    const auto lower = [&]() {
        auto value = result;
        std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return value;
    }();
    auto marker = lower.find("\\users\\");
    auto separator = '\\';
    size_t markerLength = 7;
    if (marker == std::string::npos)
    {
        marker = lower.find("/users/");
        separator = '/';
    }
    if (marker == std::string::npos)
    {
        marker = lower.find("/home/");
        markerLength = 6;
        separator = '/';
    }
    if (marker != std::string::npos)
    {
        const auto userStart = marker + markerLength;
        const auto userEnd = result.find(separator, userStart);
        if (userEnd != userStart)
        {
            const auto length = userEnd == std::string::npos ? result.size() - userStart : userEnd - userStart;
            result.replace(userStart, length, "<user>");
        }
    }
    return result;
}
