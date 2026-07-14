// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <json/json.h>

namespace winTerm::Appearance::Licensing
{
    enum class AssetCategory
    {
        Theme,
        Font,
        InheritedDependency,
    };

    struct AssetLicenseEntry
    {
        AssetCategory category{ AssetCategory::Theme };
        std::string id;
        std::string name;
        std::string author;
        std::string version;
        std::string license;
        std::string sourceProject;
        std::string sourceRevision;
        std::string sourceFile;
        std::string assetFile;
        std::string licenseFile;
        std::string sha256;
        bool bundled{ true };
    };

    struct AssetRegistryLoadResult
    {
        size_t registered{};
        size_t ignored{};
        std::vector<std::string> errors;

        bool Succeeded() const noexcept;
    };

    class AssetLicenseRegistry final
    {
    public:
        bool TryRegister(AssetLicenseEntry entry, std::string& error);
        AssetRegistryLoadResult RegisterManifest(const Json::Value& manifest, AssetCategory category);

        const AssetLicenseEntry* Find(AssetCategory category, std::string_view id) const;
        std::vector<AssetLicenseEntry> Entries() const;
        std::vector<AssetLicenseEntry> Entries(AssetCategory category) const;
        size_t Size() const noexcept;
        void Clear() noexcept;

    private:
        std::map<std::string, AssetLicenseEntry, std::less<>> _entries;
    };

    std::string_view ToString(AssetCategory value) noexcept;
}
