// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Themes/ThemeDescriptor.h"

namespace winTerm::Appearance
{
    class ThemeImporter
    {
    public:
        virtual ~ThemeImporter() = default;
        virtual ThemeImportResult Import(std::string_view content, std::string_view fileName) const = 0;

        static std::string GenerateImportedId(std::string_view displayName, std::string_view content);
        static std::string DisplayNameFromFile(std::string_view fileName);
        static std::string LeafName(std::string_view fileName);
        static void PrepareImportedTheme(ThemeDescriptor& theme, std::string_view fileName);
    };
}
