// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ThemeRegistry.h"
#include "ThemeValidator.h"

using namespace winTerm::Appearance;

namespace
{
    void ThrowIfInvalid(const ThemeDescriptor& theme)
    {
        const auto validation = ThemeValidator::Validate(theme);
        if (!validation.IsValid())
        {
            const auto errors = validation.ErrorMessages();
            throw std::invalid_argument(errors.empty() ? "The theme is invalid." : errors.front());
        }
    }
}

ThemeRegistry::ThemeRegistry(std::string fallbackThemeId) :
    _fallbackThemeId{ std::move(fallbackThemeId) }
{
}

void ThemeRegistry::RegisterBuiltIn(ThemeDescriptor theme)
{
    _registerProtected(std::move(theme), ThemeSourceType::BuiltIn);
}

void ThemeRegistry::RegisterBundled(ThemeDescriptor theme)
{
    _registerProtected(std::move(theme), ThemeSourceType::Bundled);
}

void ThemeRegistry::_registerProtected(ThemeDescriptor theme, const ThemeSourceType expectedSource)
{
    ThrowIfInvalid(theme);
    if (theme.source.type != expectedSource)
    {
        throw std::invalid_argument("The theme source does not match its registry layer.");
    }
    if (_themes.contains(theme.id))
    {
        throw std::invalid_argument("Duplicate protected theme ID: " + theme.id);
    }
    _themes.emplace(theme.id, std::move(theme));
}

std::string ThemeRegistry::_uniqueImportedId(const std::string_view requestedId) const
{
    std::string base{ requestedId };
    if (!ThemeValidator::IsValidId(base))
    {
        base = "user.imported";
    }
    if (!_themes.contains(base))
    {
        return base;
    }
    for (size_t suffix = 2; suffix < 100000; ++suffix)
    {
        auto candidate = base + "." + std::to_string(suffix);
        if (!_themes.contains(candidate))
        {
            return candidate;
        }
    }
    throw std::runtime_error("A unique imported theme ID could not be generated.");
}

std::string ThemeRegistry::RegisterImported(ThemeDescriptor theme)
{
    theme.source.type = ThemeSourceType::Imported;
    theme.id = _uniqueImportedId(theme.id);
    ThrowIfInvalid(theme);
    const auto id = theme.id;
    _themes.emplace(id, std::move(theme));
    return id;
}

bool ThemeRegistry::RemoveImported(const std::string_view id)
{
    const auto found = _themes.find(id);
    if (found == _themes.end() || found->second.source.type != ThemeSourceType::Imported)
    {
        return false;
    }
    _themes.erase(found);
    return true;
}

ThemeDescriptor ThemeRegistry::Duplicate(const std::string_view id)
{
    const auto source = GetThemeById(id);
    if (!source)
    {
        throw std::out_of_range("The theme was not found.");
    }
    auto copy = *source;
    copy.source.type = ThemeSourceType::Imported;
    copy.source.project = "winTerm user themes";
    copy.source.author = "User";
    copy.source.license = "User-provided";
    copy.source.revision.reset();
    copy.source.sourceFile.reset();
    copy.id = _uniqueImportedId("user." + copy.id);
    copy.name += " Copy";
    copy.displayName += " Copy";
    RegisterImported(copy);
    return *GetThemeById(copy.id);
}

const ThemeDescriptor* ThemeRegistry::GetThemeById(const std::string_view id) const noexcept
{
    const auto found = _themes.find(id);
    return found == _themes.end() ? nullptr : &found->second;
}

std::vector<ThemeDescriptor> ThemeRegistry::ListThemes() const
{
    std::vector<ThemeDescriptor> result;
    result.reserve(_themes.size());
    for (const auto& [id, theme] : _themes)
    {
        result.emplace_back(theme);
    }
    return result;
}

const ThemeDescriptor* ThemeRegistry::Resolve(const std::optional<std::string_view> profileThemeId,
                                              const std::optional<std::string_view> globalThemeId) const noexcept
{
    if (_temporaryPreview)
    {
        return &*_temporaryPreview;
    }
    if (profileThemeId)
    {
        if (const auto theme = GetThemeById(*profileThemeId))
        {
            return theme;
        }
    }
    if (globalThemeId)
    {
        if (const auto theme = GetThemeById(*globalThemeId))
        {
            return theme;
        }
    }
    return GetThemeById(_fallbackThemeId);
}

void ThemeRegistry::SetTemporaryPreview(std::optional<ThemeDescriptor> theme)
{
    if (theme)
    {
        ThrowIfInvalid(*theme);
    }
    _temporaryPreview = std::move(theme);
}

const std::optional<ThemeDescriptor>& ThemeRegistry::TemporaryPreview() const noexcept
{
    return _temporaryPreview;
}

const std::string& ThemeRegistry::FallbackThemeId() const noexcept
{
    return _fallbackThemeId;
}
