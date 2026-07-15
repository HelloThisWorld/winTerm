// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WorkspaceMigration.h"

#include <stdexcept>

using namespace winTerm::Workspaces;

WorkspaceMigrationResult WorkspaceMigration::Migrate(const Json::Value& document)
{
    if (!document.isObject() || !document["schemaVersion"].isUInt())
    {
        throw std::runtime_error("The workspace schema version is missing or invalid.");
    }

    WorkspaceMigrationResult result;
    result.document = document;
    result.sourceVersion = document["schemaVersion"].asUInt();
    result.targetVersion = result.sourceVersion;

    if (result.sourceVersion > WorkspaceSchemaVersion)
    {
        throw std::runtime_error("This workspace was created by a newer version of winTerm.");
    }

    while (result.targetVersion < WorkspaceSchemaVersion)
    {
        switch (result.targetVersion)
        {
        case 0:
            if (result.document["id"].isNull() && result.document["workspaceId"].isString())
            {
                result.document["id"] = result.document["workspaceId"];
                result.document.removeMember("workspaceId");
            }
            if (result.document["source"].isNull()) result.document["source"] = "user";
            if (result.document["applicationVersion"].isNull()) result.document["applicationVersion"] = "0.4.0-dev";
            if (result.document["protocolVersion"].isNull()) result.document["protocolVersion"] = 1;
            if (result.document["description"].isNull()) result.document["description"] = "";
            if (result.document["startupBehavior"].isNull())
            {
                Json::Value behavior{ Json::objectValue };
                behavior["restoreFocus"] = true;
                behavior["restoreWindowState"] = true;
                result.document["startupBehavior"] = std::move(behavior);
            }
            result.document["schemaVersion"] = 1;
            result.targetVersion = 1;
            result.changed = true;
            result.notes.emplace_back("Migrated the legacy workspace envelope to schema version 1.");
            break;
        default:
            throw std::runtime_error("The workspace cannot be migrated from its schema version.");
        }
    }
    return result;
}
