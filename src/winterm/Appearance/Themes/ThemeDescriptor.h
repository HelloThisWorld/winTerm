// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <json/json.h>

namespace winTerm::Appearance
{
    inline constexpr uint32_t CurrentThemeSchemaVersion{ 1 };
    inline constexpr size_t ThemePaletteSize{ 8 };
    inline constexpr size_t MaximumThemeFileSize{ 1024 * 1024 };
    inline constexpr size_t MaximumThemeNestingDepth{ 64 };
    inline constexpr size_t MaximumThemeStringLength{ 64 * 1024 };
    inline constexpr size_t MaximumImportedThemeCount{ 64 };

    enum class ThemeVariant
    {
        Dark,
        Light,
        HighContrast,
        Adaptive,
    };

    enum class ThemeSourceType
    {
        BuiltIn,
        Bundled,
        Imported,
        Legacy,
    };

    enum class ThemeIssueSeverity
    {
        Warning,
        Error,
    };

    struct ThemeSource
    {
        ThemeSourceType type{ ThemeSourceType::Imported };
        std::string project;
        std::string author;
        std::optional<std::string> homepage;
        std::string license;
        std::optional<std::string> revision;
        std::optional<std::string> sourceFile;
    };

    struct TerminalTheme
    {
        std::string foreground;
        std::string background;
        std::string cursorColor;
        std::string cursorTextColor;
        std::string selectionBackground;
        std::string selectionForeground;
        std::array<std::string, ThemePaletteSize> ansi;
        std::array<std::string, ThemePaletteSize> bright;
    };

    struct WindowTheme
    {
        double opacity{ 1.0 };
        bool useAcrylic{ false };
        std::string tabBarBackground;
        std::string inactiveTabBackground;
        std::string paneBorderColor;
    };

    struct ThemeDescriptor
    {
        uint32_t schemaVersion{ CurrentThemeSchemaVersion };
        std::string id;
        std::string name;
        std::string displayName;
        ThemeVariant variant{ ThemeVariant::Dark };
        ThemeSource source;
        TerminalTheme terminal;
        WindowTheme window;
        std::optional<std::string> originalFileName;

        Json::Value ToWindowsTerminalSchemeJson() const;
    };

    struct ThemeValidationIssue
    {
        ThemeIssueSeverity severity{ ThemeIssueSeverity::Error };
        std::string field;
        std::string message;
    };

    struct ThemeValidationResult
    {
        std::vector<ThemeValidationIssue> issues;

        bool IsValid() const noexcept;
        std::vector<std::string> ErrorMessages() const;
    };

    struct ThemeImportSummary
    {
        std::vector<std::string> imported;
        std::vector<std::string> ignored;
        std::vector<std::string> warnings;

        std::string ToDisplayText() const;
    };

    struct ThemeImportResult
    {
        std::vector<ThemeDescriptor> themes;
        ThemeImportSummary summary;
    };

    std::string_view ToString(ThemeVariant value) noexcept;
    std::string_view ToString(ThemeSourceType value) noexcept;
    std::optional<ThemeVariant> ThemeVariantFromString(std::string_view value) noexcept;
    std::optional<ThemeSourceType> ThemeSourceTypeFromString(std::string_view value) noexcept;
}
