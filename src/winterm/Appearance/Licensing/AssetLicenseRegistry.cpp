// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "AssetLicenseRegistry.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <tuple>
#include <utility>

using namespace winTerm::Appearance::Licensing;

namespace
{
    std::string FoldAscii(const std::string_view value)
    {
        std::string result{ value };
        std::transform(result.begin(), result.end(), result.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return result;
    }

    std::string Key(const AssetCategory category, const std::string_view id)
    {
        return std::to_string(static_cast<int>(category)) + ":" + FoldAscii(id);
    }

    std::string RequiredString(const Json::Value& entry, const char* field)
    {
        const auto& value = entry[field];
        if (!value.isString() || value.asString().empty())
        {
            throw std::runtime_error(std::string{ "The asset manifest entry is missing the required string: " } + field);
        }
        return value.asString();
    }

    std::string EntryName(const Json::Value& entry)
    {
        for (const auto field : { "name", "familyName", "family" })
        {
            const auto& value = entry[field];
            if (value.isString() && !value.asString().empty())
            {
                return value.asString();
            }
        }
        throw std::runtime_error("The asset manifest entry is missing a name.");
    }

    bool IsSha256(const std::string_view value) noexcept
    {
        return value.size() == 64 && std::all_of(value.begin(), value.end(), [](const unsigned char character) {
            return std::isxdigit(character) != 0;
        });
    }

    std::string ArrayName(const AssetCategory category)
    {
        switch (category)
        {
        case AssetCategory::Theme:
            return "themes";
        case AssetCategory::Font:
            return "fonts";
        case AssetCategory::InheritedDependency:
            return "dependencies";
        }
        return "assets";
    }
}

bool AssetRegistryLoadResult::Succeeded() const noexcept
{
    return errors.empty();
}

bool AssetLicenseRegistry::TryRegister(AssetLicenseEntry entry, std::string& error)
{
    error.clear();
    if (entry.id.empty())
    {
        error = "The asset license entry ID is required.";
        return false;
    }
    if (entry.name.empty())
    {
        error = "The asset license entry name is required.";
        return false;
    }
    if (entry.author.empty() || entry.license.empty() || entry.sourceProject.empty())
    {
        error = "The asset license entry is missing attribution metadata.";
        return false;
    }
    if (entry.category != AssetCategory::InheritedDependency)
    {
        if (entry.sourceRevision.empty() || entry.sourceFile.empty() || entry.assetFile.empty() || entry.licenseFile.empty())
        {
            error = "The bundled asset license entry is missing pinned source or file metadata.";
            return false;
        }
        if (!IsSha256(entry.sha256))
        {
            error = "The bundled asset license entry has an invalid SHA-256 value.";
            return false;
        }
    }

    const auto key = Key(entry.category, entry.id);
    if (_entries.contains(key))
    {
        error = "Duplicate asset license entry ID: " + entry.id;
        return false;
    }
    _entries.emplace(key, std::move(entry));
    return true;
}

AssetRegistryLoadResult AssetLicenseRegistry::RegisterManifest(const Json::Value& manifest, const AssetCategory category)
{
    AssetRegistryLoadResult result;
    if (!manifest.isObject())
    {
        result.errors.emplace_back("The asset manifest root must be an object.");
        return result;
    }

    const auto arrayName = ArrayName(category);
    if (!manifest[arrayName].isArray())
    {
        result.errors.emplace_back("The asset manifest must contain an array named " + arrayName + ".");
        return result;
    }

    auto candidate = *this;
    for (const auto& json : manifest[arrayName])
    {
        std::string id;
        try
        {
            if (!json.isObject())
            {
                throw std::runtime_error("The asset manifest contains an invalid entry.");
            }
            AssetLicenseEntry entry;
            entry.category = category;
            entry.id = RequiredString(json, "id");
            id = entry.id;
            entry.name = EntryName(json);
            entry.author = RequiredString(json, "author");
            entry.version = json["version"].isString() ? json["version"].asString() : RequiredString(json, "sourceRevision");
            entry.license = RequiredString(json, "license");
            entry.sourceProject = RequiredString(json, "sourceProject");
            entry.sourceRevision = RequiredString(json, "sourceRevision");
            entry.sourceFile = RequiredString(json, "sourceFile");
            entry.assetFile = RequiredString(json, "file");
            entry.licenseFile = RequiredString(json, "licenseFile");
            entry.sha256 = RequiredString(json, "sha256");
            entry.bundled = !json["bundled"].isBool() || json["bundled"].asBool();

            std::string error;
            if (!candidate.TryRegister(std::move(entry), error))
            {
                throw std::runtime_error(error);
            }
            ++result.registered;
        }
        catch (const std::exception& exception)
        {
            ++result.ignored;
            result.errors.emplace_back(id.empty() ? exception.what() : id + ": " + exception.what());
        }
    }
    if (result.errors.empty())
    {
        _entries = std::move(candidate._entries);
    }
    else
    {
        result.registered = 0;
    }
    return result;
}

const AssetLicenseEntry* AssetLicenseRegistry::Find(const AssetCategory category, const std::string_view id) const
{
    const auto found = _entries.find(Key(category, id));
    return found == _entries.end() ? nullptr : &found->second;
}

std::vector<AssetLicenseEntry> AssetLicenseRegistry::Entries() const
{
    std::vector<AssetLicenseEntry> result;
    result.reserve(_entries.size());
    for (const auto& [key, entry] : _entries)
    {
        result.emplace_back(entry);
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return std::tuple{ left.category, FoldAscii(left.name), FoldAscii(left.id) } <
               std::tuple{ right.category, FoldAscii(right.name), FoldAscii(right.id) };
    });
    return result;
}

std::vector<AssetLicenseEntry> AssetLicenseRegistry::Entries(const AssetCategory category) const
{
    auto result = Entries();
    std::erase_if(result, [category](const auto& entry) {
        return entry.category != category;
    });
    return result;
}

size_t AssetLicenseRegistry::Size() const noexcept
{
    return _entries.size();
}

void AssetLicenseRegistry::Clear() noexcept
{
    _entries.clear();
}

std::string_view winTerm::Appearance::Licensing::ToString(const AssetCategory value) noexcept
{
    switch (value)
    {
    case AssetCategory::Theme:
        return "theme";
    case AssetCategory::Font:
        return "font";
    case AssetCategory::InheritedDependency:
        return "inheritedDependency";
    }
    return "theme";
}
