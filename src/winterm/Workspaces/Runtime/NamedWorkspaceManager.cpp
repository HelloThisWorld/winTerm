// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "NamedWorkspaceManager.h"

using namespace winTerm::Workspaces;

NamedWorkspaceManager::NamedWorkspaceManager(WorkspaceStore& store) noexcept :
    _store{ store }
{
}

AtomicWriteResult NamedWorkspaceManager::SaveAs(
    const WorkspaceDescriptor& runtimeWorkspace,
    std::string id,
    std::string name,
    std::string description,
    std::vector<std::string> tags,
    const std::string_view timestamp)
{
    auto workspace = runtimeWorkspace;
    workspace.id = std::move(id);
    workspace.name = std::move(name);
    workspace.description = std::move(description);
    workspace.tags = std::move(tags);
    workspace.source = WorkspaceSource::User;
    workspace.createdAt = std::string{ timestamp };
    workspace.updatedAt = std::string{ timestamp };
    workspace.lastOpenedAt.reset();
    workspace.captureReason = "saveAsNamed";
    workspace.recoveryMetadata.reset();
    workspace.isDefault = false;
    return _store.SaveNamed(workspace);
}

AtomicWriteResult NamedWorkspaceManager::Rename(const std::string_view workspaceId, std::string name, const std::string_view timestamp)
{
    auto loaded = _store.LoadNamed(workspaceId);
    if (!loaded.workspace)
    {
        return { false, false, loaded.message };
    }
    loaded.workspace->name = std::move(name);
    loaded.workspace->updatedAt = std::string{ timestamp };
    return _store.SaveNamed(*loaded.workspace);
}

AtomicWriteResult NamedWorkspaceManager::Duplicate(
    const std::string_view workspaceId,
    std::string newId,
    std::string newName,
    const std::string_view timestamp)
{
    auto loaded = _store.LoadNamed(workspaceId);
    if (!loaded.workspace)
    {
        return { false, false, loaded.message };
    }
    loaded.workspace->id = std::move(newId);
    loaded.workspace->name = std::move(newName);
    loaded.workspace->createdAt = std::string{ timestamp };
    loaded.workspace->updatedAt = std::string{ timestamp };
    loaded.workspace->lastOpenedAt.reset();
    loaded.workspace->isDefault = false;
    loaded.workspace->captureReason = "duplicate";
    return _store.SaveNamed(*loaded.workspace);
}

bool NamedWorkspaceManager::Delete(const std::string_view workspaceId, const WorkspaceDescriptor& runtimeSnapshot, const uint64_t generation)
{
    const auto snapshot = _store.SaveSnapshot(runtimeSnapshot, generation);
    return snapshot.succeeded && _store.DeleteNamed(workspaceId);
}

bool NamedWorkspaceManager::SetDefault(const std::string_view workspaceId, const std::string_view timestamp)
{
    auto workspaces = _store.LoadAllNamed();
    bool found{};
    for (auto& workspace : workspaces)
    {
        const auto shouldBeDefault = workspace.id == workspaceId;
        found = found || shouldBeDefault;
        if (workspace.isDefault != shouldBeDefault)
        {
            workspace.isDefault = shouldBeDefault;
            workspace.updatedAt = std::string{ timestamp };
            if (!_store.SaveNamed(workspace).succeeded)
            {
                return false;
            }
        }
    }
    return found;
}
