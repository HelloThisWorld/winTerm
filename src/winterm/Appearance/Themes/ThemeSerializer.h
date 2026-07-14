// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "ThemeDescriptor.h"

#include <filesystem>

namespace winTerm::Appearance
{
    class ThemeSerializer final
    {
    public:
        static Json::Value ParseJsonDocument(std::string_view content);
        static ThemeDescriptor FromJson(const Json::Value& json);
        static ThemeDescriptor Deserialize(std::string_view content);
        static Json::Value ToJson(const ThemeDescriptor& theme, bool includeAttribution = true);
        static std::string Serialize(const ThemeDescriptor& theme, bool includeAttribution = true);
        static std::string ReadFile(const std::filesystem::path& path, size_t maximumSize = MaximumThemeFileSize);
        static void WriteFileAtomically(const std::filesystem::path& path, std::string_view content);
    };
}
