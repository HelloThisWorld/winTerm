// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ThemeValidator.h"

#include <cmath>
#include <unordered_set>

using namespace winTerm::Appearance;

namespace
{
    bool IsHexDigit(const char value) noexcept
    {
        return (value >= '0' && value <= '9') ||
               (value >= 'a' && value <= 'f') ||
               (value >= 'A' && value <= 'F');
    }

    char ToUpperHex(const char value) noexcept
    {
        return value >= 'a' && value <= 'f' ? static_cast<char>(value - ('a' - 'A')) : value;
    }

    void AddError(ThemeValidationResult& result, std::string field, std::string message)
    {
        result.issues.emplace_back(ThemeValidationIssue{ ThemeIssueSeverity::Error, std::move(field), std::move(message) });
    }

    void ValidateColor(ThemeValidationResult& result, const std::string_view field, const std::string& color)
    {
        std::string normalized;
        if (!ThemeValidator::TryNormalizeColor(color, normalized))
        {
            AddError(result, std::string{ field }, "The theme contains an invalid color value.");
        }
    }
}

bool ThemeValidator::TryNormalizeColor(const std::string_view input, std::string& normalized)
{
    normalized.clear();
    if (input.empty())
    {
        return false;
    }

    auto value = input;
    if (value.size() == 8 && value.substr(0, 2) == "0x")
    {
        value.remove_prefix(2);
    }
    else if (value.front() == '#')
    {
        value.remove_prefix(1);
    }

    if (value.size() == 3 || value.size() == 4)
    {
        const bool hasAlpha = value.size() == 4;
        for (const auto character : value)
        {
            if (!IsHexDigit(character))
            {
                return false;
            }
        }

        normalized.push_back('#');
        if (hasAlpha)
        {
            normalized.push_back(ToUpperHex(value[3]));
            normalized.push_back(ToUpperHex(value[3]));
        }
        for (size_t i = 0; i < 3; ++i)
        {
            normalized.push_back(ToUpperHex(value[i]));
            normalized.push_back(ToUpperHex(value[i]));
        }
        return true;
    }

    if (value.size() != 6 && value.size() != 8)
    {
        return false;
    }
    if (!std::all_of(value.begin(), value.end(), IsHexDigit))
    {
        return false;
    }

    normalized.push_back('#');
    std::transform(value.begin(), value.end(), std::back_inserter(normalized), ToUpperHex);
    return true;
}

bool ThemeValidator::IsValidId(const std::string_view id) noexcept
{
    if (id.size() < 3 || id.size() > 128)
    {
        return false;
    }
    if (!((id.front() >= 'a' && id.front() <= 'z') || (id.front() >= '0' && id.front() <= '9')))
    {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](const char character) {
        return (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9') ||
               character == '.' || character == '_' || character == '-';
    });
}

ThemeValidationResult ThemeValidator::Validate(const ThemeDescriptor& theme)
{
    ThemeValidationResult result;
    if (theme.schemaVersion != CurrentThemeSchemaVersion)
    {
        AddError(result, "schemaVersion", "This theme uses an unsupported schema version.");
    }
    if (!IsValidId(theme.id))
    {
        AddError(result, "id", "The theme ID is invalid.");
    }
    if (theme.name.empty() || theme.name.size() > MaximumThemeStringLength)
    {
        AddError(result, "name", "The theme name is missing or too long.");
    }
    if (theme.displayName.empty() || theme.displayName.size() > MaximumThemeStringLength)
    {
        AddError(result, "displayName", "The theme display name is missing or too long.");
    }
    if (theme.terminal.foreground.empty())
    {
        AddError(result, "terminal.foreground", "The theme foreground color is required.");
    }
    else
    {
        ValidateColor(result, "terminal.foreground", theme.terminal.foreground);
    }
    if (theme.terminal.background.empty())
    {
        AddError(result, "terminal.background", "The theme background color is required.");
    }
    else
    {
        ValidateColor(result, "terminal.background", theme.terminal.background);
    }

    const std::array<std::pair<std::string_view, const std::string*>, 4> optionalTerminalColors{
        std::pair{ "terminal.cursorColor", &theme.terminal.cursorColor },
        std::pair{ "terminal.cursorTextColor", &theme.terminal.cursorTextColor },
        std::pair{ "terminal.selectionBackground", &theme.terminal.selectionBackground },
        std::pair{ "terminal.selectionForeground", &theme.terminal.selectionForeground },
    };
    for (const auto& [field, color] : optionalTerminalColors)
    {
        if (!color->empty())
        {
            ValidateColor(result, field, *color);
        }
    }
    for (size_t i = 0; i < ThemePaletteSize; ++i)
    {
        ValidateColor(result, "terminal.ansi[" + std::to_string(i) + "]", theme.terminal.ansi[i]);
        ValidateColor(result, "terminal.bright[" + std::to_string(i) + "]", theme.terminal.bright[i]);
    }

    if (!std::isfinite(theme.window.opacity) || theme.window.opacity < 0.0 || theme.window.opacity > 1.0)
    {
        AddError(result, "window.opacity", "Theme opacity must be a finite number from 0.0 through 1.0.");
    }
    const std::array<std::pair<std::string_view, const std::string*>, 3> windowColors{
        std::pair{ "window.tabBarBackground", &theme.window.tabBarBackground },
        std::pair{ "window.inactiveTabBackground", &theme.window.inactiveTabBackground },
        std::pair{ "window.paneBorderColor", &theme.window.paneBorderColor },
    };
    for (const auto& [field, color] : windowColors)
    {
        if (!color->empty())
        {
            ValidateColor(result, field, *color);
        }
    }
    return result;
}

ThemeValidationResult ThemeValidator::ValidateCollection(const std::span<const ThemeDescriptor> themes)
{
    ThemeValidationResult result;
    std::unordered_set<std::string> ids;
    for (const auto& theme : themes)
    {
        auto themeResult = Validate(theme);
        result.issues.insert(result.issues.end(),
                             std::make_move_iterator(themeResult.issues.begin()),
                             std::make_move_iterator(themeResult.issues.end()));
        if (!ids.emplace(theme.id).second)
        {
            AddError(result, "id", "Duplicate theme ID: " + theme.id);
        }
    }
    return result;
}
