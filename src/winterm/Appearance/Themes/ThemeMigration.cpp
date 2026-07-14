// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ThemeMigration.h"

using namespace winTerm::Appearance;

ThemeMigrationResult ThemeMigration::ResolveLegacyScheme(
    const std::string_view legacySchemeName,
    const ThemeRegistry& registry,
    const std::map<std::string, std::string, std::less<>>& legacyNameToThemeId)
{
    if (const auto mapped = legacyNameToThemeId.find(legacySchemeName); mapped != legacyNameToThemeId.end())
    {
        if (registry.GetThemeById(mapped->second))
        {
            return ThemeMigrationResult{ mapped->second, false, "Migrated the legacy color scheme to a winTerm theme." };
        }
    }
    return ThemeMigrationResult{ registry.FallbackThemeId(), true, "The legacy color scheme was unavailable. winTerm used its safe fallback theme." };
}
