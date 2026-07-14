// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "ThemeRegistry.h"

namespace winTerm::Appearance
{
    struct ThemeMigrationResult
    {
        std::string themeId;
        bool usedFallback{ false };
        std::string message;
    };

    class ThemeMigration final
    {
    public:
        static ThemeMigrationResult ResolveLegacyScheme(std::string_view legacySchemeName,
                                                        const ThemeRegistry& registry,
                                                        const std::map<std::string, std::string, std::less<>>& legacyNameToThemeId);
    };
}
