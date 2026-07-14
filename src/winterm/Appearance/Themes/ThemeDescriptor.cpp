// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ThemeDescriptor.h"

#include <sstream>

using namespace winTerm::Appearance;

namespace
{
    constexpr std::array<std::string_view, 16> WindowsTerminalColorKeys{
        "black",
        "red",
        "green",
        "yellow",
        "blue",
        "purple",
        "cyan",
        "white",
        "brightBlack",
        "brightRed",
        "brightGreen",
        "brightYellow",
        "brightBlue",
        "brightPurple",
        "brightCyan",
        "brightWhite",
    };

    std::string ToOpaqueColor(const std::string& color)
    {
        if (color.size() == 9 && color.front() == '#')
        {
            return "#" + color.substr(3);
        }
        return color;
    }
}

Json::Value ThemeDescriptor::ToWindowsTerminalSchemeJson() const
{
    Json::Value json{ Json::objectValue };
    json["name"] = displayName.empty() ? name : displayName;
    json["foreground"] = ToOpaqueColor(terminal.foreground);
    json["background"] = ToOpaqueColor(terminal.background);
    json["cursorColor"] = ToOpaqueColor(terminal.cursorColor);
    json["selectionBackground"] = ToOpaqueColor(terminal.selectionBackground);

    for (size_t i = 0; i < ThemePaletteSize; ++i)
    {
        json[std::string{ WindowsTerminalColorKeys[i] }] = ToOpaqueColor(terminal.ansi[i]);
        json[std::string{ WindowsTerminalColorKeys[i + ThemePaletteSize] }] = ToOpaqueColor(terminal.bright[i]);
    }
    return json;
}

bool ThemeValidationResult::IsValid() const noexcept
{
    return std::none_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == ThemeIssueSeverity::Error;
    });
}

std::vector<std::string> ThemeValidationResult::ErrorMessages() const
{
    std::vector<std::string> messages;
    for (const auto& issue : issues)
    {
        if (issue.severity == ThemeIssueSeverity::Error)
        {
            messages.emplace_back(issue.field.empty() ? issue.message : issue.field + ": " + issue.message);
        }
    }
    return messages;
}

std::string ThemeImportSummary::ToDisplayText() const
{
    std::ostringstream output;
    if (!imported.empty())
    {
        output << "Imported:\n";
        for (const auto& item : imported)
        {
            output << "- " << item << '\n';
        }
    }
    if (!ignored.empty())
    {
        output << "\nIgnored:\n";
        for (const auto& item : ignored)
        {
            output << "- " << item << '\n';
        }
    }
    if (!warnings.empty())
    {
        output << "\nWarnings:\n";
        for (const auto& item : warnings)
        {
            output << "- " << item << '\n';
        }
    }
    return output.str();
}

std::string_view winTerm::Appearance::ToString(const ThemeVariant value) noexcept
{
    switch (value)
    {
    case ThemeVariant::Dark:
        return "dark";
    case ThemeVariant::Light:
        return "light";
    case ThemeVariant::HighContrast:
        return "highContrast";
    case ThemeVariant::Adaptive:
        return "adaptive";
    }
    return "dark";
}

std::string_view winTerm::Appearance::ToString(const ThemeSourceType value) noexcept
{
    switch (value)
    {
    case ThemeSourceType::BuiltIn:
        return "builtin";
    case ThemeSourceType::Bundled:
        return "thirdParty";
    case ThemeSourceType::Imported:
        return "imported";
    case ThemeSourceType::Legacy:
        return "legacy";
    }
    return "imported";
}

std::optional<ThemeVariant> winTerm::Appearance::ThemeVariantFromString(const std::string_view value) noexcept
{
    if (value == "dark")
    {
        return ThemeVariant::Dark;
    }
    if (value == "light")
    {
        return ThemeVariant::Light;
    }
    if (value == "highContrast")
    {
        return ThemeVariant::HighContrast;
    }
    if (value == "adaptive")
    {
        return ThemeVariant::Adaptive;
    }
    return std::nullopt;
}

std::optional<ThemeSourceType> winTerm::Appearance::ThemeSourceTypeFromString(const std::string_view value) noexcept
{
    if (value == "builtin")
    {
        return ThemeSourceType::BuiltIn;
    }
    if (value == "thirdParty" || value == "bundled")
    {
        return ThemeSourceType::Bundled;
    }
    if (value == "imported" || value == "user")
    {
        return ThemeSourceType::Imported;
    }
    if (value == "legacy")
    {
        return ThemeSourceType::Legacy;
    }
    return std::nullopt;
}
