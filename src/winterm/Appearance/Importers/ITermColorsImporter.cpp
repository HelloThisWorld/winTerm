// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ITermColorsImporter.h"
#include "PlistParser.h"
#include "ThemeImportUtils.h"
#include "../Themes/ThemeValidator.h"

using namespace winTerm::Appearance;

namespace
{
    std::optional<std::string> ReadColor(const Json::Value& plist, const std::string& key)
    {
        if (!plist.isMember(key))
        {
            return std::nullopt;
        }
        const auto color = details::DecodePlistColor(plist[key]);
        if (!color)
        {
            throw std::runtime_error("The selected file is not a valid iTerm2 color scheme.");
        }
        return color;
    }
}

ThemeImportResult ITermColorsImporter::Import(const std::string_view content, const std::string_view fileName) const
{
    const auto plist = PlistParser::Parse(content);
    if (!plist.isObject())
    {
        throw std::runtime_error("The selected file is not a valid iTerm2 color scheme.");
    }

    ThemeDescriptor theme;
    theme.terminal = details::DefaultTerminalTheme();
    const auto displayName = DisplayNameFromFile(fileName);
    theme.id = GenerateImportedId(displayName, content);
    theme.name = displayName;
    theme.displayName = displayName;
    PrepareImportedTheme(theme, fileName);
    theme.source.project = "iTerm2 color scheme import";

    ThemeImportResult result;
    bool usedPaletteFallback = false;
    for (size_t i = 0; i < 16; ++i)
    {
        const auto key = "Ansi " + std::to_string(i) + " Color";
        if (const auto color = ReadColor(plist, key))
        {
            (i < ThemePaletteSize ? theme.terminal.ansi[i] : theme.terminal.bright[i - ThemePaletteSize]) = *color;
        }
        else
        {
            usedPaletteFallback = true;
        }
    }
    theme.terminal.foreground = ReadColor(plist, "Foreground Color").value_or(theme.terminal.ansi[7]);
    theme.terminal.background = ReadColor(plist, "Background Color").value_or(theme.terminal.ansi[0]);
    theme.terminal.cursorColor = ReadColor(plist, "Cursor Color").value_or(theme.terminal.foreground);
    theme.terminal.cursorTextColor = ReadColor(plist, "Cursor Text Color").value_or(theme.terminal.background);
    theme.terminal.selectionBackground = ReadColor(plist, "Selection Color").value_or(theme.terminal.bright[0]);
    theme.terminal.selectionForeground = ReadColor(plist, "Selected Text Color").value_or(theme.terminal.foreground);
    theme.variant = details::VariantFromBackground(theme.terminal.background);
    theme.window.tabBarBackground = theme.terminal.background;
    theme.window.inactiveTabBackground = theme.terminal.ansi[0];
    theme.window.paneBorderColor = theme.terminal.bright[0];

    result.summary.imported = { "ANSI color palette", "Foreground", "Background", "Cursor", "Selection" };
    if (plist.isMember("Bold Color"))
    {
        result.summary.ignored.emplace_back("Bold color (the terminal derives bold behavior from the ANSI palette)");
    }
    if (usedPaletteFallback)
    {
        result.summary.warnings.emplace_back("Missing ANSI entries used the safe winTerm fallback palette.");
    }

    const auto validation = ThemeValidator::Validate(theme);
    if (!validation.IsValid())
    {
        throw std::runtime_error("The selected file is not a valid iTerm2 color scheme.");
    }
    result.themes.emplace_back(std::move(theme));
    return result;
}
