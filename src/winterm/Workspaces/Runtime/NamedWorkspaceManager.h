// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Persistence/WorkspaceStore.h"

namespace winTerm::Workspaces
{
    class NamedWorkspaceManager final
    {
    public:
        explicit NamedWorkspaceManager(WorkspaceStore& store) noexcept;

        AtomicWriteResult SaveAs(
            const WorkspaceDescriptor& runtimeWorkspace,
            std::string id,
            std::string name,
            std::string description,
            std::vector<std::string> tags,
            std::string_view timestamp);
        AtomicWriteResult Rename(std::string_view workspaceId, std::string name, std::string_view timestamp);
        AtomicWriteResult Duplicate(std::string_view workspaceId, std::string newId, std::string newName, std::string_view timestamp);
        bool Delete(std::string_view workspaceId, const WorkspaceDescriptor& runtimeSnapshot, uint64_t generation);
        bool SetDefault(std::string_view workspaceId, std::string_view timestamp);

    private:
        WorkspaceStore& _store;
    };
}
