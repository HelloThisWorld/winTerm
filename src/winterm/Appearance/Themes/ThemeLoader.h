// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "ThemeDescriptor.h"

#include <filesystem>

namespace winTerm::Appearance
{
    class ThemeLoader final
    {
    public:
        static ThemeImportResult Import(std::string_view content, std::string_view fileName);
        static ThemeImportResult ImportFile(const std::filesystem::path& path);
        static ThemeDescriptor LoadWinTermThemeFile(const std::filesystem::path& path);
        static void ExportFile(const ThemeDescriptor& theme, const std::filesystem::path& path, bool includeAttribution = true);
    };
}
