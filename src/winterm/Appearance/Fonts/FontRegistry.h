// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "FontDescriptor.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <json/json.h>

namespace winTerm::Appearance
{
    inline constexpr uint32_t CurrentFontManifestSchemaVersion{ 1 };
    inline constexpr size_t MaximumFontManifestFileSize{ 1024 * 1024 };

    enum class FontFilter
    {
        Recommended,
        Bundled,
        Monospaced,
        NerdFonts,
        All,
    };

    enum class FontRegistryIssueSeverity
    {
        Warning,
        Error,
    };

    struct FontRegistryIssue
    {
        FontRegistryIssueSeverity severity{ FontRegistryIssueSeverity::Error };
        std::string entryId;
        std::string field;
        std::string message;
    };

    struct FontRegistryLoadResult
    {
        size_t loaded{};
        size_t ignored{};
        std::vector<FontRegistryIssue> issues;

        bool Succeeded() const noexcept;
        std::vector<std::string> ErrorMessages() const;
    };

    struct FontSystemMergeResult
    {
        size_t added{};
        size_t duplicates{};
        size_t invalid{};
    };

    class FontRegistry final
    {
    public:
        // Font manifests may opt into repository-root-relative paths with
        // `pathBase: "repository"`. All resolved files remain constrained to
        // repositoryRoot.
        FontRegistryLoadResult LoadBundledManifestFile(const std::filesystem::path& manifestPath,
                                                       const std::filesystem::path& repositoryRoot);
        FontRegistryLoadResult LoadBundledManifest(const Json::Value& manifest,
                                                   const std::filesystem::path& manifestDirectory,
                                                   const std::filesystem::path& repositoryRoot);

        FontSystemMergeResult MergeSystemFonts(std::span<const FontDescriptor> discoveredFonts);
        void ClearSystemFonts() noexcept;

        const FontDescriptor* GetFontById(std::string_view id) const;
        const FontDescriptor* FindByFamily(std::string_view familyName) const;
        bool HasFamily(std::string_view familyName) const;
        std::vector<FontDescriptor> ListFonts(FontFilter filter = FontFilter::All) const;
        size_t Size() const noexcept;

    private:
        std::map<std::string, FontDescriptor, std::less<>> _fonts;
    };
}
