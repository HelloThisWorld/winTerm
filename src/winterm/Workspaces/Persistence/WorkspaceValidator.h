// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Model/WorkspaceDescriptor.h"

#include <string>
#include <vector>

namespace winTerm::Workspaces
{
    enum class WorkspaceIssueSeverity
    {
        Warning,
        Error,
    };

    struct WorkspaceValidationIssue
    {
        WorkspaceIssueSeverity severity{ WorkspaceIssueSeverity::Error };
        std::string code;
        std::string field;
        std::string message;
    };

    struct WorkspaceValidationResult
    {
        std::vector<WorkspaceValidationIssue> issues;
        size_t repairs{};

        bool IsValid() const noexcept;
        bool HasWarnings() const noexcept;
        std::vector<std::string> ErrorMessages() const;
    };

    class WorkspaceValidator final
    {
    public:
        static WorkspaceValidationResult Validate(const WorkspaceDescriptor& workspace);
        static WorkspaceValidationResult ValidateAndRepair(WorkspaceDescriptor& workspace);
    };
}
