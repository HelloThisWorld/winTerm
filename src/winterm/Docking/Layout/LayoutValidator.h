// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "LayoutTree.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace winTerm::Docking
{
    enum class LayoutIssueSeverity
    {
        Warning,
        Error,
    };

    struct LayoutValidationIssue
    {
        LayoutIssueSeverity severity{ LayoutIssueSeverity::Error };
        std::string code;
        std::string nodeId;
        std::string message;
    };

    struct LayoutValidationResult
    {
        std::vector<LayoutValidationIssue> issues;
        size_t nodeCount{};
        size_t paneCount{};
        size_t emptySlotCount{};
        size_t maximumDepth{};

        bool IsValid() const noexcept;
        std::vector<std::string> ErrorMessages() const;
    };

    struct LayoutValidationOptions
    {
        size_t maximumDepth{ Workspaces::MaximumWorkspaceLayoutDepth };
        size_t maximumPanes{ Workspaces::MaximumWorkspacePanesPerTab };
        std::unordered_set<std::string> liveSessionPaneIds;
        bool requireLiveSessionForEveryPane{ false };
    };

    class LayoutValidator
    {
    public:
        static LayoutValidationResult Validate(
            const LayoutNodePtr& root,
            const LayoutValidationOptions& options = {});
    };
}
