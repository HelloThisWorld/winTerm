// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Model/WorkspaceDescriptor.h"

#include <filesystem>
#include <json/json.h>

namespace winTerm::Workspaces
{
    class WorkspaceSerializer final
    {
    public:
        static Json::Value ParseJsonDocument(std::string_view content);
        static Json::Value ToJson(const WorkspaceDescriptor& workspace);
        static WorkspaceDescriptor FromJson(const Json::Value& json, bool validate = true);
        static std::string Serialize(const WorkspaceDescriptor& workspace);
        static WorkspaceDescriptor Deserialize(std::string_view content, bool validate = true);
        static std::string ReadFile(const std::filesystem::path& path, size_t maximumSize = MaximumWorkspaceFileSize);
    };

    using WorkspaceDeserializer = WorkspaceSerializer;
}
