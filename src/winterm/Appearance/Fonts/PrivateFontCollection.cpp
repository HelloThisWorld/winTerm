// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PrivateFontCollection.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <system_error>
#include <unordered_set>
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

    bool IsSha256(const std::string_view value) noexcept
    {
        return value.size() == 64 && std::all_of(value.begin(), value.end(), [](const unsigned char character) {
            return std::isxdigit(character) != 0;
        });
    }

    void AddIssue(PrivateFontCollectionLoadResult& result,
                  const PrivateFontCollectionIssueSeverity severity,
                  std::string fontId,
                  std::string message)
    {
        result.issues.emplace_back(PrivateFontCollectionIssue{ severity, std::move(fontId), std::move(message) });
    }
}

bool PrivateFontCollectionLoadResult::HasUsableFonts() const noexcept
{
    return loaded != 0;
}

PrivateFontCollection::PrivateFontCollection(std::string fallbackFamily) :
    _fallbackFamily{ std::move(fallbackFamily) }
{
    if (_fallbackFamily.empty())
    {
        _fallbackFamily = "Cascadia Mono";
    }
}

PrivateFontCollectionLoadResult PrivateFontCollection::Load(const std::span<const FontDescriptor> bundledFonts,
                                                            const std::filesystem::path& repositoryRoot,
                                                            const bool enabled)
{
    Clear();
    _enabled = enabled;
    PrivateFontCollectionLoadResult result;
    if (!enabled)
    {
        return result;
    }

    std::error_code error;
    const auto root = std::filesystem::weakly_canonical(repositoryRoot, error);
    if (error || !std::filesystem::is_directory(root, error) || error)
    {
        AddIssue(result, PrivateFontCollectionIssueSeverity::Error, {}, "The private font repository root could not be resolved.");
        return result;
    }

    std::unordered_set<std::string> ids;
    for (const auto& descriptor : bundledFonts)
    {
        const auto id = descriptor.id;
        if (!descriptor.IsBundled() || descriptor.id.empty() || descriptor.familyName.empty())
        {
            ++result.ignored;
            AddIssue(result, PrivateFontCollectionIssueSeverity::Error, id, "The private font descriptor is invalid or is not bundled.");
            continue;
        }
        if (!ids.emplace(FoldAscii(descriptor.id)).second)
        {
            ++result.ignored;
            AddIssue(result, PrivateFontCollectionIssueSeverity::Warning, id, "A duplicate private font descriptor was ignored.");
            continue;
        }
        if (!descriptor.file)
        {
            ++result.ignored;
            AddIssue(result, PrivateFontCollectionIssueSeverity::Error, id, "The private font file is not specified.");
            continue;
        }
        if (!IsSha256(descriptor.sha256))
        {
            ++result.ignored;
            AddIssue(result, PrivateFontCollectionIssueSeverity::Error, id, "The private font SHA-256 value is invalid.");
            continue;
        }

        const auto candidate = descriptor.file->is_absolute() ? *descriptor.file : root / *descriptor.file;
        error.clear();
        const auto resolved = std::filesystem::weakly_canonical(candidate, error);
        if (error || !IsWithin(resolved, root))
        {
            ++result.ignored;
            AddIssue(result, PrivateFontCollectionIssueSeverity::Error, id, "The private font path leaves the repository root.");
            continue;
        }
        error.clear();
        if (!std::filesystem::is_regular_file(resolved, error) || error)
        {
            ++result.ignored;
            AddIssue(result, PrivateFontCollectionIssueSeverity::Error, id, "The private font file is missing or cannot be read.");
            continue;
        }

        auto accepted = descriptor;
        accepted.file = resolved;
        _fonts.emplace_back(std::move(accepted));
        ++result.loaded;
    }
    std::sort(_fonts.begin(), _fonts.end(), [](const auto& left, const auto& right) {
        return left.id < right.id;
    });
    return result;
}

void PrivateFontCollection::Clear() noexcept
{
    _fonts.clear();
    _enabled = false;
}

bool PrivateFontCollection::IsEnabled() const noexcept
{
    return _enabled;
}

bool PrivateFontCollection::IsReady() const noexcept
{
    return _enabled && !_fonts.empty();
}

const std::vector<FontDescriptor>& PrivateFontCollection::Fonts() const noexcept
{
    return _fonts;
}

const std::string& PrivateFontCollection::FallbackFamily() const noexcept
{
    return _fallbackFamily;
}

std::string PrivateFontCollection::ResolveFamily(const std::string_view requestedFamily) const
{
    if (_enabled)
    {
        const auto requested = FoldAscii(requestedFamily);
        const auto found = std::find_if(_fonts.begin(), _fonts.end(), [&](const auto& descriptor) {
            return FoldAscii(descriptor.familyName) == requested;
        });
        if (found != _fonts.end())
        {
            return found->familyName;
        }
    }
    return _fallbackFamily;
}
