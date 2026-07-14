// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ThemeImportUtils.h"
#include "../Themes/ThemeValidator.h"

#include <bit>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>

using namespace winTerm::Appearance;

namespace
{
    std::optional<double> Component(const Json::Value& value, const std::initializer_list<const char*> keys)
    {
        for (const auto key : keys)
        {
            if (value[key].isNumeric())
            {
                const auto component = value[key].asDouble();
                if (!std::isfinite(component) || component < 0.0 || component > 1.0)
                {
                    throw std::runtime_error("The theme contains a color component outside the supported range.");
                }
                return component;
            }
        }
        return std::nullopt;
    }

    std::string ToHexColor(const double red, const double green, const double blue, const double alpha = 1.0)
    {
        const auto byte = [](const double value) {
            return static_cast<unsigned int>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
        };
        std::ostringstream output;
        output << '#' << std::uppercase << std::hex << std::setfill('0');
        if (alpha < 0.999999)
        {
            output << std::setw(2) << byte(alpha);
        }
        output << std::setw(2) << byte(red)
               << std::setw(2) << byte(green)
               << std::setw(2) << byte(blue);
        return output.str();
    }

    std::optional<std::vector<uint8_t>> DecodeBase64(const std::string_view input)
    {
        static constexpr std::string_view alphabet{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };
        std::array<int16_t, 256> table;
        table.fill(-1);
        for (size_t i = 0; i < alphabet.size(); ++i)
        {
            table[static_cast<uint8_t>(alphabet[i])] = static_cast<int16_t>(i);
        }

        std::vector<uint8_t> output;
        output.reserve(input.size() * 3 / 4);
        uint32_t accumulator = 0;
        unsigned int bits = 0;
        for (const unsigned char character : input)
        {
            if (std::isspace(character))
            {
                continue;
            }
            if (character == '=')
            {
                break;
            }
            const auto value = table[character];
            if (value < 0)
            {
                return std::nullopt;
            }
            accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                output.push_back(static_cast<uint8_t>((accumulator >> bits) & 0xFF));
            }
        }
        return output;
    }

    std::optional<std::array<double, 4>> DecodeTextComponents(const std::vector<uint8_t>& data)
    {
        std::string printable;
        printable.reserve(data.size() + 1);
        for (const auto value : data)
        {
            printable.push_back(value >= 0x20 && value <= 0x7E ? static_cast<char>(value) : ' ');
        }
        printable.push_back('\0');

        for (size_t i = 0; i + 5 < printable.size(); ++i)
        {
            if (!(printable[i] == '-' || printable[i] == '.' || std::isdigit(static_cast<unsigned char>(printable[i]))))
            {
                continue;
            }
            const char* cursor = printable.c_str() + i;
            char* end = nullptr;
            std::array<double, 4> components{ 0.0, 0.0, 0.0, 1.0 };
            bool valid = true;
            for (size_t component = 0; component < 3; ++component)
            {
                components[component] = std::strtod(cursor, &end);
                if (end == cursor || !std::isfinite(components[component]) || components[component] < 0.0 || components[component] > 1.0)
                {
                    valid = false;
                    break;
                }
                cursor = end;
                while (*cursor == ' ')
                {
                    ++cursor;
                }
            }
            if (valid)
            {
                char* alphaEnd = nullptr;
                const auto alpha = std::strtod(cursor, &alphaEnd);
                if (alphaEnd != cursor && std::isfinite(alpha) && alpha >= 0.0 && alpha <= 1.0)
                {
                    components[3] = alpha;
                }
                return components;
            }
        }
        return std::nullopt;
    }

    std::optional<std::array<double, 4>> DecodeLegacyArchivedComponents(const std::vector<uint8_t>& data)
    {
        static constexpr std::array<uint8_t, 4> marker{ 'f', 'f', 'f', 'f' };
        const auto found = std::search(data.begin(), data.end(), marker.begin(), marker.end());
        if (found == data.end())
        {
            return std::nullopt;
        }
        auto position = static_cast<size_t>(std::distance(data.begin(), found)) + marker.size();
        std::array<double, 4> components{ 0.0, 0.0, 0.0, 1.0 };
        for (size_t component = 0; component < 3; ++component)
        {
            if (position + 5 > data.size() || data[position] != 0x83)
            {
                return std::nullopt;
            }
            ++position;
            uint32_t raw = static_cast<uint32_t>(data[position]) |
                           (static_cast<uint32_t>(data[position + 1]) << 8) |
                           (static_cast<uint32_t>(data[position + 2]) << 16) |
                           (static_cast<uint32_t>(data[position + 3]) << 24);
            position += 4;
            const auto value = std::bit_cast<float>(raw);
            if (!std::isfinite(value) || value < 0.0f || value > 1.0f)
            {
                return std::nullopt;
            }
            components[component] = value;
        }
        if (position + 5 <= data.size() && data[position] == 0x83)
        {
            ++position;
            uint32_t raw = static_cast<uint32_t>(data[position]) |
                           (static_cast<uint32_t>(data[position + 1]) << 8) |
                           (static_cast<uint32_t>(data[position + 2]) << 16) |
                           (static_cast<uint32_t>(data[position + 3]) << 24);
            const auto value = std::bit_cast<float>(raw);
            if (std::isfinite(value) && value >= 0.0f && value <= 1.0f)
            {
                components[3] = value;
            }
        }
        return components;
    }

    std::optional<std::array<double, 4>> DecodeArchivedComponents(const std::string_view base64)
    {
        const auto bytes = DecodeBase64(base64);
        if (!bytes || bytes->empty() || bytes->size() > MaximumThemeStringLength)
        {
            return std::nullopt;
        }
        if (const auto text = DecodeTextComponents(*bytes))
        {
            return text;
        }
        return DecodeLegacyArchivedComponents(*bytes);
    }

    std::optional<uint8_t> HexByte(const std::string_view value)
    {
        unsigned int result = 0;
        const auto parseNibble = [](const char character) -> std::optional<unsigned int> {
            if (character >= '0' && character <= '9')
                return character - '0';
            if (character >= 'A' && character <= 'F')
                return character - 'A' + 10;
            if (character >= 'a' && character <= 'f')
                return character - 'a' + 10;
            return std::nullopt;
        };
        const auto high = parseNibble(value[0]);
        const auto low = parseNibble(value[1]);
        if (!high || !low)
        {
            return std::nullopt;
        }
        result = (*high << 4) | *low;
        return static_cast<uint8_t>(result);
    }
}

TerminalTheme winTerm::Appearance::details::DefaultTerminalTheme()
{
    TerminalTheme theme;
    theme.foreground = "#CCCCCC";
    theme.background = "#0C0C0C";
    theme.cursorColor = "#FFFFFF";
    theme.cursorTextColor = "#0C0C0C";
    theme.selectionBackground = "#264F78";
    theme.selectionForeground = "#FFFFFF";
    theme.ansi = { "#0C0C0C", "#C50F1F", "#13A10E", "#C19C00", "#0037DA", "#881798", "#3A96DD", "#CCCCCC" };
    theme.bright = { "#767676", "#E74856", "#16C60C", "#F9F1A5", "#3B78FF", "#B4009E", "#61D6D6", "#F2F2F2" };
    return theme;
}

std::optional<std::string> winTerm::Appearance::details::DecodePlistColor(const Json::Value& value)
{
    if (value.isObject())
    {
        const auto red = Component(value, { "Red Component", "red", "Red" });
        const auto green = Component(value, { "Green Component", "green", "Green" });
        const auto blue = Component(value, { "Blue Component", "blue", "Blue" });
        if (!red || !green || !blue)
        {
            return std::nullopt;
        }
        const auto alpha = Component(value, { "Alpha Component", "alpha", "Alpha" }).value_or(1.0);
        return ToHexColor(*red, *green, *blue, alpha);
    }
    if (!value.isString())
    {
        return std::nullopt;
    }

    std::string normalized;
    if (ThemeValidator::TryNormalizeColor(value.asString(), normalized))
    {
        return normalized;
    }
    if (const auto components = DecodeArchivedComponents(value.asString()))
    {
        return ToHexColor((*components)[0], (*components)[1], (*components)[2], (*components)[3]);
    }
    return std::nullopt;
}

ThemeVariant winTerm::Appearance::details::VariantFromBackground(const std::string_view background) noexcept
{
    std::string normalized;
    if (!ThemeValidator::TryNormalizeColor(background, normalized))
    {
        return ThemeVariant::Dark;
    }
    const auto offset = normalized.size() == 9 ? 3u : 1u;
    const auto red = HexByte(std::string_view{ normalized }.substr(offset, 2)).value_or(0);
    const auto green = HexByte(std::string_view{ normalized }.substr(offset + 2, 2)).value_or(0);
    const auto blue = HexByte(std::string_view{ normalized }.substr(offset + 4, 2)).value_or(0);
    const auto luminance = (0.2126 * red) + (0.7152 * green) + (0.0722 * blue);
    return luminance >= 160.0 ? ThemeVariant::Light : ThemeVariant::Dark;
}
