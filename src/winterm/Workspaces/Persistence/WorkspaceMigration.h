// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Model/WorkspaceDescriptor.h"

#include <json/json.h>
#include <vector>

namespace winTerm::Workspaces
{
    struct WorkspaceMigrationResult
    {
        Json::Value document;
        uint32_t sourceVersion{};
        uint32_t targetVersion{};
        bool changed{ false };
        std::vector<std::string> notes;
    };

    class WorkspaceMigration final
    {
    public:
        static WorkspaceMigrationResult Migrate(const Json::Value& document);
    };
}
