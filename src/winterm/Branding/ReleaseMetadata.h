// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <cstdint>
#include <string_view>

#if __has_include("ReleaseMetadata.generated.h")
#include "ReleaseMetadata.generated.h"
#endif

#ifndef WINTERM_BUILD_COMMIT_SHA
#define WINTERM_BUILD_COMMIT_SHA L"development"
#endif

#ifndef WINTERM_BUILD_TIMESTAMP
#define WINTERM_BUILD_TIMESTAMP L"not available"
#endif

#ifndef WINTERM_BUILD_WORKFLOW_RUN_ID
#define WINTERM_BUILD_WORKFLOW_RUN_ID L"local"
#endif

namespace winTerm::Branding
{
    inline constexpr std::wstring_view ApplicationVersion{ L"1.0.2" };
    inline constexpr std::wstring_view ReleaseChannel{ L"Stable" };
    inline constexpr std::wstring_view CommitSha{ WINTERM_BUILD_COMMIT_SHA };
    inline constexpr std::wstring_view BuildTimestamp{ WINTERM_BUILD_TIMESTAMP };
    inline constexpr std::wstring_view WorkflowRunId{ WINTERM_BUILD_WORKFLOW_RUN_ID };
    inline constexpr std::wstring_view MicrosoftTerminalUpstreamRevision{ L"1cea42d433253d95c4487a3037db48197b5e72f4" };
    inline constexpr uint32_t WorkspaceSchemaVersion{ 2 };
    inline constexpr uint32_t DockingModelVersion{ 1 };
    inline constexpr uint32_t ShellProtocolVersion{ 1 };
    inline constexpr uint32_t ThemeSchemaVersion{ 1 };
    inline constexpr uint32_t UpdateManifestSchemaVersion{ 1 };
}
