// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "FontRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <tuple>
#include <utility>

using namespace winTerm::Appearance;

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

    std::optional<std::filesystem::path> ResolveRepositoryPath(const std::string_view relativePath,
                                                               const std::filesystem::path& base,
                                                               const std::filesystem::path& repositoryRoot)
    {
        const std::filesystem::path relative{ std::string{ relativePath } };
        if (relative.empty() || relative.is_absolute() || relative.has_root_name() || relative.has_root_directory())
        {
            return std::nullopt;
        }

        std::error_code error;
        const auto root = std::filesystem::weakly_canonical(repositoryRoot, error);
        if (error)
        {
            return std::nullopt;
        }
        auto candidate = std::filesystem::absolute(base / relative, error).lexically_normal();
        if (error || !IsWithin(candidate, root))
        {
            return std::nullopt;
        }
        error.clear();
        if (std::filesystem::exists(candidate, error) && !error)
        {
            const auto canonicalCandidate = std::filesystem::weakly_canonical(candidate, error);
            if (error || !IsWithin(canonicalCandidate, root))
            {
                return std::nullopt;
            }
            candidate = canonicalCandidate;
        }
        return candidate;
    }

    bool IsSha256(const std::string_view value) noexcept
    {
        return value.size() == 64 && std::all_of(value.begin(), value.end(), [](const unsigned char character) {
            return std::isxdigit(character) != 0;
        });
    }

    bool IsValidId(const std::string_view value) noexcept
    {
        if (value.empty() || value.size() > 128)
        {
            return false;
        }
        return std::all_of(value.begin(), value.end(), [](const unsigned char character) {
            return (character >= 'a' && character <= 'z') ||
                   (character >= 'A' && character <= 'Z') ||
                   (character >= '0' && character <= '9') ||
                   character == '.' || character == '-' || character == '_';
        });
    }

    bool IsRecommendedFamily(const std::string_view familyName)
    {
        const auto value = FoldAscii(familyName);
        return value == "cascadia mono nf" || value == "cascadia mono" || value == "cascadia code" ||
               value == "jetbrains mono" || value == "fira code";
    }

    std::string RequiredString(const Json::Value& object, const char* field)
    {
        const auto& value = object[field];
        if (!value.isString() || value.asString().empty())
        {
            throw std::runtime_error(std::string{ "The font manifest entry is missing the required string: " } + field);
        }
        return value.asString();
    }

    std::string FirstString(const Json::Value& object, const std::initializer_list<const char*> fields)
    {
        for (const auto field : fields)
        {
            const auto& value = object[field];
            if (value.isString() && !value.asString().empty())
            {
                return value.asString();
            }
        }
        return {};
    }

    FontPropertyState ReadPropertyState(const Json::Value& object, const char* field)
    {
        const auto& value = object[field];
        if (value.isNull())
        {
            return FontPropertyState::Unknown;
        }
        if (value.isBool())
        {
            return value.asBool() ? FontPropertyState::Yes : FontPropertyState::No;
        }
        if (value.isString())
        {
            if (const auto state = FontPropertyStateFromString(FoldAscii(value.asString())))
            {
                return *state;
            }
        }
        throw std::runtime_error(std::string{ "The font capability field is invalid: " } + field);
    }

    GlyphCoverageEstimate ReadCoverage(const Json::Value& object, const char* field)
    {
        const auto& value = object[field];
        if (value.isNull())
        {
            return GlyphCoverageEstimate::Unknown;
        }
        if (value.isBool())
        {
            return value.asBool() ? GlyphCoverageEstimate::PresentInSample : GlyphCoverageEstimate::NotPresentInSample;
        }
        if (value.isString())
        {
            const auto text = value.asString();
            if (const auto estimate = GlyphCoverageEstimateFromString(text))
            {
                return *estimate;
            }
            if (const auto estimate = GlyphCoverageEstimateFromString(FoldAscii(text)))
            {
                return *estimate;
            }
        }
        throw std::runtime_error(std::string{ "The font coverage field is invalid: " } + field);
    }

    std::vector<VariableFontAxis> ReadVariableAxes(const Json::Value& entry)
    {
        const auto& json = entry["variableAxes"];
        if (json.isNull())
        {
            return {};
        }
        if (!json.isArray())
        {
            throw std::runtime_error("The variableAxes font field must be an array.");
        }

        std::vector<VariableFontAxis> axes;
        axes.reserve(json.size());
        for (const auto& axisJson : json)
        {
            if (!axisJson.isObject() || !axisJson["minimum"].isNumeric() ||
                !axisJson["maximum"].isNumeric() || !axisJson["default"].isNumeric())
            {
                throw std::runtime_error("A variable font axis is invalid.");
            }
            VariableFontAxis axis;
            axis.tag = RequiredString(axisJson, "tag");
            axis.name = axisJson["name"].isString() ? axisJson["name"].asString() : axis.tag;
            axis.minimum = axisJson["minimum"].asDouble();
            axis.maximum = axisJson["maximum"].asDouble();
            axis.defaultValue = axisJson["default"].asDouble();
            if (!std::isfinite(axis.minimum) || !std::isfinite(axis.maximum) || !std::isfinite(axis.defaultValue) ||
                axis.minimum > axis.maximum || axis.defaultValue < axis.minimum || axis.defaultValue > axis.maximum)
            {
                throw std::runtime_error("A variable font axis range is invalid.");
            }
            axes.emplace_back(std::move(axis));
        }
        return axes;
    }

    void AddIssue(FontRegistryLoadResult& result,
                  const FontRegistryIssueSeverity severity,
                  std::string entryId,
                  std::string field,
                  std::string message)
    {
        result.issues.emplace_back(FontRegistryIssue{ severity, std::move(entryId), std::move(field), std::move(message) });
    }

    std::string ReadFile(const std::filesystem::path& path)
    {
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error)
        {
            throw std::runtime_error("The font manifest could not be read.");
        }
        if (size > MaximumFontManifestFileSize)
        {
            throw std::runtime_error("The font manifest exceeds the maximum allowed size.");
        }

        std::ifstream input{ path, std::ios::binary };
        if (!input)
        {
            throw std::runtime_error("The font manifest could not be read.");
        }
        std::string content(static_cast<size_t>(size), '\0');
        if (!content.empty())
        {
            input.read(content.data(), static_cast<std::streamsize>(content.size()));
            if (input.gcount() != static_cast<std::streamsize>(content.size()))
            {
                throw std::runtime_error("The font manifest could not be read completely.");
            }
        }
        return content;
    }

    Json::Value ParseManifest(const std::string& content)
    {
        Json::CharReaderBuilder builder;
        builder["allowComments"] = false;
        builder["allowSpecialFloats"] = false;
        builder["collectComments"] = false;
        builder["failIfExtra"] = true;
        builder["rejectDupKeys"] = true;
        builder["skipBom"] = true;
        builder["stackLimit"] = static_cast<Json::UInt64>(64);
        builder["strictRoot"] = true;

        auto reader = std::unique_ptr<Json::CharReader>{ builder.newCharReader() };
        Json::Value root;
        std::string errors;
        if (!reader->parse(content.data(), content.data() + content.size(), &root, &errors))
        {
            throw std::runtime_error("The font manifest is not valid JSON.");
        }
        return root;
    }

    std::string GeneratedSystemId(const FontDescriptor& descriptor)
    {
        const auto source = FoldAscii(descriptor.familyName + "\n" + descriptor.fullName);
        uint64_t hash{ 14695981039346656037ull };
        for (const auto character : source)
        {
            hash ^= static_cast<unsigned char>(character);
            hash *= 1099511628211ull;
        }
        std::ostringstream stream;
        stream << "system." << std::hex << std::setfill('0') << std::setw(16) << hash;
        return stream.str();
    }

    bool MatchesFilter(const FontDescriptor& descriptor, const FontFilter filter) noexcept
    {
        switch (filter)
        {
        case FontFilter::Recommended:
            return descriptor.recommended;
        case FontFilter::Bundled:
            return descriptor.IsBundled();
        case FontFilter::Monospaced:
            return descriptor.monospaced == FontPropertyState::Yes;
        case FontFilter::NerdFonts:
            return descriptor.nerdFontCoverage == GlyphCoverageEstimate::PresentInSample;
        case FontFilter::All:
            return true;
        }
        return true;
    }
}

bool FontRegistryLoadResult::Succeeded() const noexcept
{
    return std::none_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == FontRegistryIssueSeverity::Error;
    });
}

std::vector<std::string> FontRegistryLoadResult::ErrorMessages() const
{
    std::vector<std::string> messages;
    for (const auto& issue : issues)
    {
        if (issue.severity == FontRegistryIssueSeverity::Error)
        {
            auto prefix = issue.entryId.empty() ? std::string{} : issue.entryId + ": ";
            if (!issue.field.empty())
            {
                prefix += issue.field + ": ";
            }
            messages.emplace_back(prefix + issue.message);
        }
    }
    return messages;
}

FontRegistryLoadResult FontRegistry::LoadBundledManifestFile(const std::filesystem::path& manifestPath,
                                                              const std::filesystem::path& repositoryRoot)
{
    FontRegistryLoadResult result;
    try
    {
        const auto manifest = ParseManifest(ReadFile(manifestPath));
        return LoadBundledManifest(manifest, manifestPath.parent_path(), repositoryRoot);
    }
    catch (const std::exception& exception)
    {
        AddIssue(result, FontRegistryIssueSeverity::Error, {}, {}, exception.what());
    }
    return result;
}

FontRegistryLoadResult FontRegistry::LoadBundledManifest(const Json::Value& manifest,
                                                          const std::filesystem::path& manifestDirectory,
                                                          const std::filesystem::path& repositoryRoot)
{
    FontRegistryLoadResult result;
    if (!manifest.isObject() || !manifest["schemaVersion"].isUInt() ||
        manifest["schemaVersion"].asUInt() != CurrentFontManifestSchemaVersion)
    {
        AddIssue(result, FontRegistryIssueSeverity::Error, {}, "schemaVersion", "The font manifest schema version is missing or unsupported.");
        return result;
    }
    if (!manifest["fonts"].isArray())
    {
        AddIssue(result, FontRegistryIssueSeverity::Error, {}, "fonts", "The font manifest must contain a fonts array.");
        return result;
    }

    const auto pathBase = manifest["pathBase"].isString() ? manifest["pathBase"].asString() : "manifest";
    const auto base = pathBase == "repository" ? repositoryRoot : manifestDirectory;
    if (pathBase != "repository" && pathBase != "manifest")
    {
        AddIssue(result, FontRegistryIssueSeverity::Error, {}, "pathBase", "The font manifest path base is not supported.");
        return result;
    }

    for (const auto& entry : manifest["fonts"])
    {
        std::string id;
        try
        {
            if (!entry.isObject())
            {
                throw std::runtime_error("The font manifest contains an invalid entry.");
            }

            id = RequiredString(entry, "id");
            if (!IsValidId(id))
            {
                throw std::runtime_error("The font ID is invalid.");
            }
            const auto key = FoldAscii(id);
            if (_fonts.contains(key))
            {
                ++result.ignored;
                AddIssue(result, FontRegistryIssueSeverity::Error, id, "id", "Duplicate font ID: " + id);
                continue;
            }

            FontDescriptor descriptor;
            descriptor.id = id;
            descriptor.familyName = FirstString(entry, { "familyName", "family", "name" });
            if (descriptor.familyName.empty())
            {
                throw std::runtime_error("The font manifest entry is missing a family name.");
            }
            descriptor.fullName = FirstString(entry, { "fullName", "name" });
            if (descriptor.fullName.empty())
            {
                descriptor.fullName = descriptor.familyName;
            }
            descriptor.source = FontSourceType::Bundled;
            descriptor.recommended = (entry["recommended"].isBool() && entry["recommended"].asBool()) ||
                                     IsRecommendedFamily(descriptor.familyName);
            descriptor.monospaced = ReadPropertyState(entry, "monospaced");
            descriptor.nerdFontCoverage = ReadCoverage(entry, "nerdFont");
            descriptor.powerlineCoverage = ReadCoverage(entry, "powerline");
            descriptor.boxDrawingCoverage = ReadCoverage(entry, "boxDrawing");
            descriptor.emojiCoverage = ReadCoverage(entry, "emoji");
            descriptor.variableAxes = ReadVariableAxes(entry);
            descriptor.version = RequiredString(entry, "version");
            descriptor.author = RequiredString(entry, "author");
            descriptor.license = RequiredString(entry, "license");
            descriptor.sourceProject = RequiredString(entry, "sourceProject");
            descriptor.sourceRevision = RequiredString(entry, "sourceRevision");
            descriptor.sha256 = RequiredString(entry, "sha256");
            if (!IsSha256(descriptor.sha256))
            {
                throw std::runtime_error("The bundled font SHA-256 value is invalid.");
            }

            const auto fileValue = RequiredString(entry, "file");
            descriptor.file = ResolveRepositoryPath(fileValue, base, repositoryRoot);
            if (!descriptor.file)
            {
                throw std::runtime_error("The bundled font path is absolute or leaves the repository root.");
            }
            std::error_code error;
            if (!std::filesystem::is_regular_file(*descriptor.file, error) || error)
            {
                const auto& packageFile = entry["packageFile"];
                if (packageFile.isString())
                {
                    const auto packagedPath = ResolveRepositoryPath(packageFile.asString(), repositoryRoot, repositoryRoot);
                    if (!packagedPath)
                    {
                        throw std::runtime_error("The packaged font path is absolute or leaves the package root.");
                    }
                    error.clear();
                    if (std::filesystem::is_regular_file(*packagedPath, error) && !error)
                    {
                        descriptor.file = packagedPath;
                    }
                }
                error.clear();
                if (!std::filesystem::is_regular_file(*descriptor.file, error) || error)
                {
                    throw std::runtime_error("The bundled font file is missing or cannot be read.");
                }
            }

            const auto licenseValue = RequiredString(entry, "licenseFile");
            descriptor.licenseFile = ResolveRepositoryPath(licenseValue, base, repositoryRoot);
            if (!descriptor.licenseFile)
            {
                throw std::runtime_error("The bundled font license path is absolute or leaves the repository root.");
            }
            error.clear();
            if (!std::filesystem::is_regular_file(*descriptor.licenseFile, error) || error)
            {
                const auto& packageLicenseFile = entry["packageLicenseFile"];
                if (packageLicenseFile.isString())
                {
                    const auto packagedPath = ResolveRepositoryPath(packageLicenseFile.asString(), repositoryRoot, repositoryRoot);
                    if (!packagedPath)
                    {
                        throw std::runtime_error("The packaged font license path is absolute or leaves the package root.");
                    }
                    error.clear();
                    if (std::filesystem::is_regular_file(*packagedPath, error) && !error)
                    {
                        descriptor.licenseFile = packagedPath;
                    }
                }
                error.clear();
                if (!std::filesystem::is_regular_file(*descriptor.licenseFile, error) || error)
                {
                    throw std::runtime_error("The bundled font license file is missing or cannot be read.");
                }
            }

            _fonts.emplace(key, std::move(descriptor));
            ++result.loaded;
        }
        catch (const std::exception& exception)
        {
            ++result.ignored;
            AddIssue(result, FontRegistryIssueSeverity::Error, std::move(id), {}, exception.what());
        }
    }
    return result;
}

FontSystemMergeResult FontRegistry::MergeSystemFonts(const std::span<const FontDescriptor> discoveredFonts)
{
    std::vector<FontDescriptor> ordered{ discoveredFonts.begin(), discoveredFonts.end() };
    std::sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
        return std::tie(left.familyName, left.fullName, left.id) < std::tie(right.familyName, right.fullName, right.id);
    });

    FontSystemMergeResult result;
    for (auto& descriptor : ordered)
    {
        if (descriptor.familyName.empty())
        {
            ++result.invalid;
            continue;
        }
        if (descriptor.fullName.empty())
        {
            descriptor.fullName = descriptor.familyName;
        }
        descriptor.source = FontSourceType::System;
        descriptor.recommended = descriptor.recommended || IsRecommendedFamily(descriptor.familyName);
        if (descriptor.id.empty())
        {
            descriptor.id = GeneratedSystemId(descriptor);
        }
        const auto key = FoldAscii(descriptor.id);
        if (_fonts.contains(key))
        {
            ++result.duplicates;
            continue;
        }
        _fonts.emplace(key, std::move(descriptor));
        ++result.added;
    }
    return result;
}

void FontRegistry::ClearSystemFonts() noexcept
{
    std::erase_if(_fonts, [](const auto& item) {
        return item.second.IsSystem();
    });
}

const FontDescriptor* FontRegistry::GetFontById(const std::string_view id) const
{
    const auto found = _fonts.find(FoldAscii(id));
    return found == _fonts.end() ? nullptr : &found->second;
}

const FontDescriptor* FontRegistry::FindByFamily(const std::string_view familyName) const
{
    const auto requested = FoldAscii(familyName);
    const FontDescriptor* best = nullptr;
    for (const auto& [id, descriptor] : _fonts)
    {
        if (FoldAscii(descriptor.familyName) != requested)
        {
            continue;
        }
        if (!best ||
            (descriptor.IsBundled() && best->IsSystem()) ||
            (descriptor.source == best->source && descriptor.recommended && !best->recommended) ||
            (descriptor.source == best->source && descriptor.recommended == best->recommended && descriptor.id < best->id))
        {
            best = &descriptor;
        }
    }
    return best;
}

bool FontRegistry::HasFamily(const std::string_view familyName) const
{
    return FindByFamily(familyName) != nullptr;
}

std::vector<FontDescriptor> FontRegistry::ListFonts(const FontFilter filter) const
{
    std::vector<FontDescriptor> result;
    result.reserve(_fonts.size());
    for (const auto& [id, descriptor] : _fonts)
    {
        if (MatchesFilter(descriptor, filter))
        {
            result.emplace_back(descriptor);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        if (left.recommended != right.recommended)
        {
            return left.recommended > right.recommended;
        }
        const auto leftFamily = FoldAscii(left.familyName);
        const auto rightFamily = FoldAscii(right.familyName);
        return std::tie(leftFamily, left.fullName, left.id) < std::tie(rightFamily, right.fullName, right.id);
    });
    return result;
}

size_t FontRegistry::Size() const noexcept
{
    return _fonts.size();
}
