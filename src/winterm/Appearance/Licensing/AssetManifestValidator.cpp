// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "AssetManifestValidator.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <unordered_set>
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

    std::wstring FoldPathComponent(const std::filesystem::path& component)
    {
        auto value = component.wstring();
        std::transform(value.begin(), value.end(), value.begin(), [](const wchar_t character) {
            return static_cast<wchar_t>(std::towlower(character));
        });
        return value;
    }

    bool IsWithin(const std::filesystem::path& candidate, const std::filesystem::path& root)
    {
        auto candidatePart = candidate.begin();
        for (auto rootPart = root.begin(); rootPart != root.end(); ++rootPart, ++candidatePart)
        {
            if (candidatePart == candidate.end() || FoldPathComponent(*candidatePart) != FoldPathComponent(*rootPart))
            {
                return false;
            }
        }
        return true;
    }

    void AddIssue(AssetManifestValidationResult& result,
                  const AssetManifestIssueSeverity severity,
                  std::string entryId,
                  std::string field,
                  std::string message)
    {
        result.issues.emplace_back(AssetManifestIssue{ severity, std::move(entryId), std::move(field), std::move(message) });
    }

    std::optional<std::string> StringField(const Json::Value& entry,
                                           const char* field,
                                           const std::string& id,
                                           AssetManifestValidationResult& result)
    {
        const auto& value = entry[field];
        if (!value.isString() || value.asString().empty())
        {
            AddIssue(result,
                     AssetManifestIssueSeverity::Error,
                     id,
                     field,
                     std::string{ "The asset manifest entry is missing the required string: " } + field);
            return std::nullopt;
        }
        return value.asString();
    }

    std::optional<std::string> AssetName(const Json::Value& entry,
                                         const AssetCategory category,
                                         const std::string& id,
                                         AssetManifestValidationResult& result)
    {
        if (entry["name"].isString() && !entry["name"].asString().empty())
        {
            return entry["name"].asString();
        }
        if (category == AssetCategory::Font)
        {
            for (const auto field : { "familyName", "family" })
            {
                if (entry[field].isString() && !entry[field].asString().empty())
                {
                    return entry[field].asString();
                }
            }
        }
        AddIssue(result, AssetManifestIssueSeverity::Error, id, "name", "The asset manifest entry is missing a name.");
        return std::nullopt;
    }

    bool IsValidAssetId(const std::string_view value) noexcept
    {
        if (value.size() < 3 || value.size() > 128 ||
            !((value.front() >= 'a' && value.front() <= 'z') || (value.front() >= '0' && value.front() <= '9')))
        {
            return false;
        }
        return std::all_of(value.begin(), value.end(), [](const char character) {
            return (character >= 'a' && character <= 'z') ||
                   (character >= '0' && character <= '9') ||
                   character == '.' || character == '_' || character == '-';
        });
    }

    bool HasAllowedExtension(const std::filesystem::path& path, const AssetCategory category)
    {
        const auto extension = FoldAscii(path.extension().string());
        switch (category)
        {
        case AssetCategory::Theme:
            return extension == ".json";
        case AssetCategory::Font:
            return extension == ".ttf" || extension == ".otf" || extension == ".ttc";
        case AssetCategory::InheritedDependency:
            return true;
        }
        return false;
    }

    void ValidateLocalFile(AssetManifestValidationResult& result,
                           const std::string& id,
                           const std::string_view field,
                           const std::string& relativePath,
                           const std::string_view pathBase,
                           const std::filesystem::path& manifestDirectory,
                           const std::filesystem::path& repositoryRoot,
                           const bool validateExtension,
                           const AssetCategory category)
    {
        const auto resolved = AssetManifestValidator::ResolvePath(relativePath, pathBase, manifestDirectory, repositoryRoot);
        if (!resolved)
        {
            AddIssue(result,
                     AssetManifestIssueSeverity::Error,
                     id,
                     std::string{ field },
                     "The asset path must be relative and remain within the repository root.");
            return;
        }
        if (validateExtension && !HasAllowedExtension(*resolved, category))
        {
            AddIssue(result,
                     AssetManifestIssueSeverity::Error,
                     id,
                     std::string{ field },
                     "The bundled asset file extension is not supported.");
            return;
        }

        std::error_code error;
        if (!std::filesystem::is_regular_file(*resolved, error) || error)
        {
            AddIssue(result,
                     AssetManifestIssueSeverity::Error,
                     id,
                     std::string{ field },
                     field == "licenseFile" ? "The asset license file is missing or cannot be read."
                                            : "The bundled asset file is missing or cannot be read.");
        }
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

    Json::Value ParseFile(const std::filesystem::path& manifestPath)
    {
        std::error_code error;
        const auto size = std::filesystem::file_size(manifestPath, error);
        if (error)
        {
            throw std::runtime_error("The asset manifest could not be read.");
        }
        if (size > MaximumAssetManifestFileSize)
        {
            throw std::runtime_error("The asset manifest exceeds the maximum allowed size.");
        }

        std::ifstream input{ manifestPath, std::ios::binary };
        if (!input)
        {
            throw std::runtime_error("The asset manifest could not be read.");
        }
        std::string content(static_cast<size_t>(size), '\0');
        if (!content.empty())
        {
            input.read(content.data(), static_cast<std::streamsize>(content.size()));
            if (input.gcount() != static_cast<std::streamsize>(content.size()))
            {
                throw std::runtime_error("The asset manifest could not be read completely.");
            }
        }

        Json::CharReaderBuilder builder;
        builder["allowComments"] = false;
        builder["allowSpecialFloats"] = false;
        builder["collectComments"] = false;
        builder["failIfExtra"] = true;
        builder["rejectDupKeys"] = true;
        builder["skipBom"] = true;
        builder["stackLimit"] = Json::UInt64{ 64 };
        builder["strictRoot"] = true;
        auto reader = std::unique_ptr<Json::CharReader>{ builder.newCharReader() };
        Json::Value root;
        std::string errors;
        if (!reader->parse(content.data(), content.data() + content.size(), &root, &errors))
        {
            throw std::runtime_error("The asset manifest is not valid JSON.");
        }
        return root;
    }
}

bool AssetManifestValidationResult::IsValid() const noexcept
{
    return std::none_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == AssetManifestIssueSeverity::Error;
    });
}

std::vector<std::string> AssetManifestValidationResult::ErrorMessages() const
{
    std::vector<std::string> messages;
    for (const auto& issue : issues)
    {
        if (issue.severity != AssetManifestIssueSeverity::Error)
        {
            continue;
        }
        std::string prefix;
        if (!issue.entryId.empty())
        {
            prefix += issue.entryId + ": ";
        }
        if (!issue.field.empty())
        {
            prefix += issue.field + ": ";
        }
        messages.emplace_back(prefix + issue.message);
    }
    return messages;
}

AssetManifestValidationResult AssetManifestValidator::ValidateFile(const std::filesystem::path& manifestPath,
                                                                   const AssetCategory category,
                                                                   const std::filesystem::path& repositoryRoot)
{
    AssetManifestValidationResult result;
    try
    {
        std::error_code error;
        const auto root = std::filesystem::weakly_canonical(repositoryRoot, error);
        if (error || !std::filesystem::is_directory(root, error) || error)
        {
            AddIssue(result, AssetManifestIssueSeverity::Error, {}, {}, "The repository root could not be resolved.");
            return result;
        }
        const auto resolvedManifest = std::filesystem::weakly_canonical(manifestPath, error);
        if (error || !IsWithin(resolvedManifest, root))
        {
            AddIssue(result, AssetManifestIssueSeverity::Error, {}, {}, "The asset manifest must remain within the repository root.");
            return result;
        }
        return Validate(ParseFile(resolvedManifest), category, resolvedManifest.parent_path(), root);
    }
    catch (const std::exception& exception)
    {
        AddIssue(result, AssetManifestIssueSeverity::Error, {}, {}, exception.what());
    }
    return result;
}

AssetManifestValidationResult AssetManifestValidator::Validate(const Json::Value& manifest,
                                                               const AssetCategory category,
                                                               const std::filesystem::path& manifestDirectory,
                                                               const std::filesystem::path& repositoryRoot)
{
    AssetManifestValidationResult result;
    if (!manifest.isObject())
    {
        AddIssue(result, AssetManifestIssueSeverity::Error, {}, {}, "The asset manifest root must be an object.");
        return result;
    }
    if (!manifest["schemaVersion"].isUInt() || manifest["schemaVersion"].asUInt() != CurrentAssetManifestSchemaVersion)
    {
        AddIssue(result, AssetManifestIssueSeverity::Error, {}, "schemaVersion", "The asset manifest schema version is missing or unsupported.");
        return result;
    }

    const auto pathBase = manifest["pathBase"].isString() ? manifest["pathBase"].asString() : "manifest";
    if (pathBase != "manifest" && pathBase != "repository")
    {
        AddIssue(result, AssetManifestIssueSeverity::Error, {}, "pathBase", "The asset manifest path base is not supported.");
        return result;
    }

    const auto arrayName = ArrayName(category);
    const auto& entries = manifest[arrayName];
    if (!entries.isArray())
    {
        AddIssue(result, AssetManifestIssueSeverity::Error, {}, arrayName, "The asset manifest must contain an array named " + arrayName + ".");
        return result;
    }

    std::unordered_set<std::string> ids;
    for (const auto& entry : entries)
    {
        if (!entry.isObject())
        {
            AddIssue(result, AssetManifestIssueSeverity::Error, {}, arrayName, "The asset manifest contains an invalid entry.");
            continue;
        }

        std::string id;
        if (const auto value = StringField(entry, "id", {}, result))
        {
            id = *value;
            if (!IsValidAssetId(id))
            {
                AddIssue(result, AssetManifestIssueSeverity::Error, id, "id", "The asset ID is invalid.");
            }
            if (!ids.emplace(FoldAscii(id)).second)
            {
                AddIssue(result, AssetManifestIssueSeverity::Error, id, "id", "Duplicate asset ID: " + id);
            }
        }

        AssetName(entry, category, id, result);
        const auto file = StringField(entry, "file", id, result);
        StringField(entry, "author", id, result);
        StringField(entry, "license", id, result);
        const auto licenseFile = StringField(entry, "licenseFile", id, result);
        StringField(entry, "sourceProject", id, result);
        StringField(entry, "sourceRevision", id, result);
        StringField(entry, "sourceFile", id, result);
        if (category == AssetCategory::Font)
        {
            StringField(entry, "version", id, result);
        }

        if (const auto sha256 = StringField(entry, "sha256", id, result); sha256 && !IsValidSha256(*sha256))
        {
            AddIssue(result, AssetManifestIssueSeverity::Error, id, "sha256", "The asset SHA-256 value must contain exactly 64 hexadecimal characters.");
        }
        if (file)
        {
            ValidateLocalFile(result, id, "file", *file, pathBase, manifestDirectory, repositoryRoot, true, category);
        }
        if (licenseFile)
        {
            ValidateLocalFile(result, id, "licenseFile", *licenseFile, pathBase, manifestDirectory, repositoryRoot, false, category);
        }
    }
    return result;
}

bool AssetManifestValidator::IsValidSha256(const std::string_view value) noexcept
{
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](const unsigned char character) {
        return std::isxdigit(character) != 0;
    });
}

std::optional<std::filesystem::path> AssetManifestValidator::ResolvePath(const std::string_view relativePath,
                                                                         const std::string_view pathBase,
                                                                         const std::filesystem::path& manifestDirectory,
                                                                         const std::filesystem::path& repositoryRoot)
{
    const std::filesystem::path relative{ std::string{ relativePath } };
    if (relative.empty() || relative.is_absolute() || relative.has_root_name() || relative.has_root_directory())
    {
        return std::nullopt;
    }
    if (pathBase != "manifest" && pathBase != "repository")
    {
        return std::nullopt;
    }

    std::error_code error;
    const auto root = std::filesystem::absolute(repositoryRoot, error).lexically_normal();
    if (error)
    {
        return std::nullopt;
    }
    const auto& base = pathBase == "repository" ? repositoryRoot : manifestDirectory;
    auto candidate = std::filesystem::absolute(base / relative, error).lexically_normal();
    if (error || !IsWithin(candidate, root))
    {
        return std::nullopt;
    }

    error.clear();
    if (std::filesystem::exists(candidate, error) && !error)
    {
        const auto canonicalRoot = std::filesystem::weakly_canonical(root, error);
        if (error)
        {
            return std::nullopt;
        }
        const auto canonicalCandidate = std::filesystem::weakly_canonical(candidate, error);
        if (error || !IsWithin(canonicalCandidate, canonicalRoot))
        {
            return std::nullopt;
        }
        candidate = canonicalCandidate;
    }
    return candidate;
}
