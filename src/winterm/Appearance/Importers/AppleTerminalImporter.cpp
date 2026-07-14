// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "AppleTerminalImporter.h"
#include "PlistParser.h"
#include "ThemeImportUtils.h"
#include "../Themes/ThemeValidator.h"

using namespace winTerm::Appearance;

namespace
{
    std::optional<std::string> ReadFirstColor(const Json::Value& plist, const std::initializer_list<const char*> keys)
    {
        for (const auto key : keys)
        {
            if (!plist.isMember(key))
            {
                continue;
            }
            const auto color = details::DecodePlistColor(plist[key]);
            if (!color)
            {
                throw std::runtime_error("The selected file is not a valid macOS Terminal profile.");
            }
            return color;
        }
        return std::nullopt;
    }

    std::string ProfileName(const Json::Value& plist, const std::string_view fileName)
    {
        for (const auto key : { "name", "ProfileName", "WindowSettingsName" })
        {
            if (plist[key].isString() && !plist[key].asString().empty())
            {
                return plist[key].asString();
            }
        }
        return ThemeImporter::DisplayNameFromFile(fileName);
    }
}

ThemeImportResult AppleTerminalImporter::Import(const std::string_view content, const std::string_view fileName) const
{
    const auto plist = PlistParser::Parse(content);
    if (!plist.isObject())
    {
        throw std::runtime_error("The selected file is not a valid macOS Terminal profile.");
    }

    ThemeDescriptor theme;
    theme.terminal = details::DefaultTerminalTheme();
    const auto displayName = ProfileName(plist, fileName);
    theme.id = GenerateImportedId(displayName, content);
    theme.name = displayName;
    theme.displayName = displayName;
    PrepareImportedTheme(theme, fileName);
    theme.source.project = "macOS Terminal profile import";

    constexpr std::array<std::array<const char*, 2>, 8> ansiKeys{ {
        { "ANSIBlackColor", "blackColour" },
        { "ANSIRedColor", "redColour" },
        { "ANSIGreenColor", "greenColour" },
        { "ANSIYellowColor", "yellowColour" },
        { "ANSIBlueColor", "blueColour" },
        { "ANSIMagentaColor", "magentaColour" },
        { "ANSICyanColor", "cyanColour" },
        { "ANSIWhiteColor", "whiteColour" },
    } };
    constexpr std::array<std::array<const char*, 2>, 8> brightKeys{ {
        { "ANSIBrightBlackColor", "brightBlackColour" },
        { "ANSIBrightRedColor", "brightRedColour" },
        { "ANSIBrightGreenColor", "brightGreenColour" },
        { "ANSIBrightYellowColor", "brightYellowColour" },
        { "ANSIBrightBlueColor", "brightBlueColour" },
        { "ANSIBrightMagentaColor", "brightMagentaColour" },
        { "ANSIBrightCyanColor", "brightCyanColour" },
        { "ANSIBrightWhiteColor", "brightWhiteColour" },
    } };

    bool usedPaletteFallback = false;
    for (size_t i = 0; i < ThemePaletteSize; ++i)
    {
        if (const auto color = ReadFirstColor(plist, { ansiKeys[i][0], ansiKeys[i][1] }))
            theme.terminal.ansi[i] = *color;
        else
            usedPaletteFallback = true;
        if (const auto color = ReadFirstColor(plist, { brightKeys[i][0], brightKeys[i][1] }))
            theme.terminal.bright[i] = *color;
        else
            usedPaletteFallback = true;
    }

    theme.terminal.foreground = ReadFirstColor(plist, { "TextColor", "ForegroundColor" }).value_or(theme.terminal.ansi[7]);
    theme.terminal.background = ReadFirstColor(plist, { "BackgroundColor" }).value_or(theme.terminal.ansi[0]);
    theme.terminal.cursorColor = ReadFirstColor(plist, { "CursorColor" }).value_or(theme.terminal.foreground);
    theme.terminal.cursorTextColor = theme.terminal.background;
    theme.terminal.selectionBackground = ReadFirstColor(plist, { "SelectionColor" }).value_or(theme.terminal.bright[0]);
    theme.terminal.selectionForeground = theme.terminal.foreground;
    theme.variant = details::VariantFromBackground(theme.terminal.background);
    theme.window.tabBarBackground = theme.terminal.background;
    theme.window.inactiveTabBackground = theme.terminal.ansi[0];
    theme.window.paneBorderColor = theme.terminal.bright[0];
    for (const auto opacityKey : { "BackgroundAlpha", "BackgroundOpacity" })
    {
        if (plist[opacityKey].isNumeric())
        {
            theme.window.opacity = plist[opacityKey].asDouble();
            break;
        }
    }

    ThemeImportResult result;
    result.summary.imported = { "Color palette", "Foreground", "Background", "Cursor", "Selection" };
    result.summary.ignored = { "Startup command", "Shell command", "Environment variables", "Keyboard mappings", "Window behavior", "macOS-specific settings" };
    if (usedPaletteFallback)
    {
        result.summary.warnings.emplace_back("Missing ANSI entries used the safe winTerm fallback palette.");
    }

    const auto validation = ThemeValidator::Validate(theme);
    if (!validation.IsValid())
    {
        throw std::runtime_error("The selected file is not a valid macOS Terminal profile.");
    }
    result.themes.emplace_back(std::move(theme));
    return result;
}
