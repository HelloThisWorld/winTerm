// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "AssetLicenseRegistry.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <json/json.h>

namespace winTerm::Appearance::Licensing
{
    inline constexpr uint32_t CurrentAssetManifestSchemaVersion{ 1 };
    inline constexpr size_t MaximumAssetManifestFileSize{ 1024 * 1024 };

    enum class AssetManifestIssueSeverity
    {
        Warning,
        Error,
    };

    struct AssetManifestIssue
    {
        AssetManifestIssueSeverity severity{ AssetManifestIssueSeverity::Error };
        std::string entryId;
        std::string field;
        std::string message;
    };

    struct AssetManifestValidationResult
    {
        std::vector<AssetManifestIssue> issues;

        bool IsValid() const noexcept;
        std::vector<std::string> ErrorMessages() const;
    };

    class AssetManifestValidator final
    {
    public:
        static AssetManifestValidationResult ValidateFile(const std::filesystem::path& manifestPath,
                                                          AssetCategory category,
                                                          const std::filesystem::path& repositoryRoot);
        static AssetManifestValidationResult Validate(const Json::Value& manifest,
                                                      AssetCategory category,
                                                      const std::filesystem::path& manifestDirectory,
                                                      const std::filesystem::path& repositoryRoot);

        static bool IsValidSha256(std::string_view value) noexcept;

        // The returned path is lexical/canonical only when it remains within
        // repositoryRoot. The caller still decides whether a missing path is an
        // error. SHA-256 content verification is intentionally performed by the
        // build script so this core does not add a cryptography dependency.
        static std::optional<std::filesystem::path> ResolvePath(std::string_view relativePath,
                                                                std::string_view pathBase,
                                                                const std::filesystem::path& manifestDirectory,
                                                                const std::filesystem::path& repositoryRoot);
    };
}
