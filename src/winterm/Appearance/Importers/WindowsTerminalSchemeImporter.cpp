// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WindowsTerminalSchemeImporter.h"
#include "ThemeImportUtils.h"
#include "../Themes/ThemeSerializer.h"
#include "../Themes/ThemeValidator.h"

using namespace winTerm::Appearance;

namespace
{
    constexpr std::array<const char*, 8> AnsiKeys{ "black", "red", "green", "yellow", "blue", "purple", "cyan", "white" };
    constexpr std::array<const char*, 8> BrightKeys{ "brightBlack", "brightRed", "brightGreen", "brightYellow", "brightBlue", "brightPurple", "brightCyan", "brightWhite" };

    std::string ReadRequiredColor(const Json::Value& scheme, const char* key)
    {
        if (!scheme[key].isString())
        {
            throw std::runtime_error("The Windows Terminal color scheme is missing a required color.");
        }
        std::string normalized;
        if (!ThemeValidator::TryNormalizeColor(scheme[key].asString(), normalized))
        {
            throw std::runtime_error("The theme contains an invalid color value.");
        }
        return normalized;
    }

    std::string ReadOptionalColor(const Json::Value& scheme, const char* key, const std::string& fallback)
    {
        if (scheme[key].isNull())
        {
            return fallback;
        }
        return ReadRequiredColor(scheme, key);
    }

    std::string CompactJson(const Json::Value& value)
    {
        Json::StreamWriterBuilder builder;
        builder["commentStyle"] = "None";
        builder["indentation"] = "";
        return Json::writeString(builder, value);
    }
}

ThemeImportResult WindowsTerminalSchemeImporter::Import(const std::string_view content, const std::string_view fileName) const
{
    const auto document = ThemeSerializer::ParseJsonDocument(content);
    std::vector<Json::Value> schemes;
    if (document.isObject() && document["schemes"].isArray())
    {
        if (document["schemes"].size() > MaximumImportedThemeCount)
        {
            throw std::runtime_error("The imported file contains too many color schemes.");
        }
        for (const auto& scheme : document["schemes"])
        {
            schemes.emplace_back(scheme);
        }
    }
    else if (document.isArray())
    {
        if (document.size() > MaximumImportedThemeCount)
        {
            throw std::runtime_error("The imported file contains too many color schemes.");
        }
        for (const auto& scheme : document)
        {
            schemes.emplace_back(scheme);
        }
    }
    else if (document.isObject())
    {
        schemes.emplace_back(document);
    }
    else
    {
        throw std::runtime_error("The selected file does not contain a Windows Terminal color scheme.");
    }
    if (schemes.empty())
    {
        throw std::runtime_error("The selected file does not contain a Windows Terminal color scheme.");
    }

    ThemeImportResult result;
    for (const auto& scheme : schemes)
    {
        if (!scheme.isObject() || !scheme["name"].isString() || scheme["name"].asString().empty())
        {
            throw std::runtime_error("A Windows Terminal color scheme is missing its name.");
        }

        ThemeDescriptor theme;
        theme.terminal = details::DefaultTerminalTheme();
        theme.name = scheme["name"].asString();
        theme.displayName = theme.name;
        theme.id = GenerateImportedId(theme.displayName, CompactJson(scheme));
        PrepareImportedTheme(theme, fileName);
        theme.source.project = "Windows Terminal color scheme import";

        theme.terminal.foreground = ReadRequiredColor(scheme, "foreground");
        theme.terminal.background = ReadRequiredColor(scheme, "background");
        theme.terminal.cursorColor = ReadOptionalColor(scheme, "cursorColor", theme.terminal.foreground);
        theme.terminal.cursorTextColor = theme.terminal.background;
        theme.terminal.selectionBackground = ReadOptionalColor(scheme, "selectionBackground", theme.terminal.bright[0]);
        theme.terminal.selectionForeground = theme.terminal.foreground;
        for (size_t i = 0; i < ThemePaletteSize; ++i)
        {
            theme.terminal.ansi[i] = ReadRequiredColor(scheme, AnsiKeys[i]);
            theme.terminal.bright[i] = ReadRequiredColor(scheme, BrightKeys[i]);
        }
        theme.variant = details::VariantFromBackground(theme.terminal.background);
        theme.window.tabBarBackground = theme.terminal.background;
        theme.window.inactiveTabBackground = theme.terminal.ansi[0];
        theme.window.paneBorderColor = theme.terminal.bright[0];

        const auto validation = ThemeValidator::Validate(theme);
        if (!validation.IsValid())
        {
            throw std::runtime_error("The selected file contains an invalid Windows Terminal color scheme.");
        }
        result.themes.emplace_back(std::move(theme));
    }

    result.summary.imported = { std::to_string(result.themes.size()) + (result.themes.size() == 1 ? " color scheme" : " color schemes") };
    if (document.isObject())
    {
        for (const auto key : { "profiles", "actions", "keybindings", "commandline", "defaultProfile" })
        {
            if (document.isMember(key))
            {
                result.summary.ignored.emplace_back(std::string{ key } + " settings");
            }
        }
    }
    return result;
}
