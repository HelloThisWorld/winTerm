// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ThemeSerializer.h"
#include "ThemeValidator.h"

#include <fstream>
#include <sstream>
#include <til/io.h>

using namespace winTerm::Appearance;

namespace
{
    const Json::Value& RequiredObject(const Json::Value& parent, const char* key)
    {
        const auto& value = parent[key];
        if (!value.isObject())
        {
            throw std::runtime_error(std::string{ "The theme is missing the required object: " } + key);
        }
        return value;
    }

    std::string RequiredString(const Json::Value& parent, const char* key)
    {
        const auto& value = parent[key];
        if (!value.isString() || value.asString().empty())
        {
            throw std::runtime_error(std::string{ "The theme is missing the required string: " } + key);
        }
        return value.asString();
    }

    std::optional<std::string> OptionalString(const Json::Value& parent, const char* key)
    {
        const auto& value = parent[key];
        if (value.isNull())
        {
            return std::nullopt;
        }
        if (!value.isString())
        {
            throw std::runtime_error(std::string{ "The theme field must be a string: " } + key);
        }
        return value.asString();
    }

    std::string NormalizeRequiredColor(const Json::Value& parent, const char* key)
    {
        const auto value = RequiredString(parent, key);
        std::string normalized;
        if (!ThemeValidator::TryNormalizeColor(value, normalized))
        {
            throw std::runtime_error("The theme contains an invalid color value.");
        }
        return normalized;
    }

    std::string NormalizeOptionalColor(const Json::Value& parent, const char* key, const std::string& fallback)
    {
        const auto& value = parent[key];
        if (value.isNull())
        {
            return fallback;
        }
        if (!value.isString())
        {
            throw std::runtime_error("The theme contains an invalid color value.");
        }
        std::string normalized;
        if (!ThemeValidator::TryNormalizeColor(value.asString(), normalized))
        {
            throw std::runtime_error("The theme contains an invalid color value.");
        }
        return normalized;
    }

    void CheckJsonLimits(const Json::Value& value, const size_t depth)
    {
        if (depth > MaximumThemeNestingDepth)
        {
            throw std::runtime_error("The imported theme exceeds the maximum nesting depth.");
        }
        if (value.isString() && value.asString().size() > MaximumThemeStringLength)
        {
            throw std::runtime_error("The imported theme contains a string that is too long.");
        }
        if (value.isArray() || value.isObject())
        {
            for (const auto& child : value)
            {
                CheckJsonLimits(child, depth + 1);
            }
        }
    }

    Json::Value StringArray(const std::array<std::string, ThemePaletteSize>& values)
    {
        Json::Value result{ Json::arrayValue };
        for (const auto& value : values)
        {
            result.append(value);
        }
        return result;
    }

    std::string LeafName(const std::string_view value)
    {
        const auto separator = value.find_last_of("/\\");
        return std::string{ separator == std::string_view::npos ? value : value.substr(separator + 1) };
    }
}

Json::Value ThemeSerializer::ParseJsonDocument(const std::string_view content)
{
    if (content.size() > MaximumThemeFileSize)
    {
        throw std::runtime_error("The imported file exceeds the maximum allowed size.");
    }

    Json::CharReaderBuilder builder;
    builder["allowComments"] = false;
    builder["allowDroppedNullPlaceholders"] = false;
    builder["allowNumericKeys"] = false;
    builder["allowSingleQuotes"] = false;
    builder["allowSpecialFloats"] = false;
    builder["collectComments"] = false;
    builder["failIfExtra"] = true;
    builder["rejectDupKeys"] = true;
    builder["skipBom"] = true;
    builder["stackLimit"] = static_cast<Json::UInt64>(MaximumThemeNestingDepth);
    builder["strictRoot"] = true;

    auto reader = std::unique_ptr<Json::CharReader>{ builder.newCharReader() };
    Json::Value root;
    std::string errors;
    if (!reader->parse(content.data(), content.data() + content.size(), &root, &errors))
    {
        throw std::runtime_error("The selected file is not valid JSON.");
    }
    CheckJsonLimits(root, 0);
    return root;
}

ThemeDescriptor ThemeSerializer::FromJson(const Json::Value& json)
{
    if (!json.isObject())
    {
        throw std::runtime_error("The selected file is not a valid winTerm theme.");
    }

    ThemeDescriptor theme;
    const auto& schemaVersion = json["schemaVersion"];
    if (!schemaVersion.isUInt())
    {
        throw std::runtime_error("The theme schema version is missing or invalid.");
    }
    theme.schemaVersion = schemaVersion.asUInt();
    theme.id = RequiredString(json, "id");
    theme.name = RequiredString(json, "name");
    theme.displayName = json["displayName"].isString() ? json["displayName"].asString() : theme.name;

    const auto variant = ThemeVariantFromString(RequiredString(json, "variant"));
    if (!variant)
    {
        throw std::runtime_error("The theme variant is not supported.");
    }
    theme.variant = *variant;

    const auto& source = RequiredObject(json, "source");
    const auto sourceType = ThemeSourceTypeFromString(RequiredString(source, "type"));
    if (!sourceType)
    {
        throw std::runtime_error("The theme source type is not supported.");
    }
    theme.source.type = *sourceType;
    theme.source.project = RequiredString(source, "project");
    theme.source.author = RequiredString(source, "author");
    theme.source.homepage = OptionalString(source, "homepage");
    theme.source.license = RequiredString(source, "license");
    theme.source.revision = OptionalString(source, "revision");
    theme.source.sourceFile = OptionalString(source, "sourceFile");

    const auto& terminal = RequiredObject(json, "terminal");
    theme.terminal.foreground = NormalizeRequiredColor(terminal, "foreground");
    theme.terminal.background = NormalizeRequiredColor(terminal, "background");
    theme.terminal.cursorColor = NormalizeOptionalColor(terminal, "cursorColor", theme.terminal.foreground);
    theme.terminal.cursorTextColor = NormalizeOptionalColor(terminal, "cursorTextColor", theme.terminal.background);
    theme.terminal.selectionBackground = NormalizeOptionalColor(terminal, "selectionBackground", theme.terminal.cursorColor);
    theme.terminal.selectionForeground = NormalizeOptionalColor(terminal, "selectionForeground", theme.terminal.foreground);

    const auto& ansi = terminal["ansi"];
    const auto& bright = terminal["bright"];
    if (!ansi.isArray() || ansi.size() != ThemePaletteSize || !bright.isArray() || bright.size() != ThemePaletteSize)
    {
        throw std::runtime_error("The theme must contain exactly eight ANSI colors and eight bright ANSI colors.");
    }
    for (Json::ArrayIndex i = 0; i < ThemePaletteSize; ++i)
    {
        if (!ansi[i].isString() || !bright[i].isString() ||
            !ThemeValidator::TryNormalizeColor(ansi[i].asString(), theme.terminal.ansi[i]) ||
            !ThemeValidator::TryNormalizeColor(bright[i].asString(), theme.terminal.bright[i]))
        {
            throw std::runtime_error("The theme contains an invalid color value.");
        }
    }

    const auto& window = RequiredObject(json, "window");
    if (window["opacity"].isNumeric())
    {
        theme.window.opacity = window["opacity"].asDouble();
    }
    else if (!window["opacity"].isNull())
    {
        throw std::runtime_error("Theme opacity must be a finite number from 0.0 through 1.0.");
    }
    if (window["useAcrylic"].isBool())
    {
        theme.window.useAcrylic = window["useAcrylic"].asBool();
    }
    else if (!window["useAcrylic"].isNull())
    {
        throw std::runtime_error("The theme acrylic setting must be a boolean value.");
    }
    theme.window.tabBarBackground = NormalizeOptionalColor(window, "tabBarBackground", theme.terminal.background);
    theme.window.inactiveTabBackground = NormalizeOptionalColor(window, "inactiveTabBackground", theme.window.tabBarBackground);
    theme.window.paneBorderColor = NormalizeOptionalColor(window, "paneBorderColor", theme.terminal.selectionBackground);

    if (const auto originalFileName = OptionalString(json, "originalFileName"))
    {
        theme.originalFileName = LeafName(*originalFileName);
    }

    const auto validation = ThemeValidator::Validate(theme);
    if (!validation.IsValid())
    {
        const auto errors = validation.ErrorMessages();
        throw std::runtime_error(errors.empty() ? "The selected file is not a valid winTerm theme." : errors.front());
    }
    return theme;
}

ThemeDescriptor ThemeSerializer::Deserialize(const std::string_view content)
{
    return FromJson(ParseJsonDocument(content));
}

Json::Value ThemeSerializer::ToJson(const ThemeDescriptor& theme, const bool includeAttribution)
{
    const auto validation = ThemeValidator::Validate(theme);
    if (!validation.IsValid())
    {
        const auto errors = validation.ErrorMessages();
        throw std::runtime_error(errors.empty() ? "The theme cannot be serialized." : errors.front());
    }

    Json::Value json{ Json::objectValue };
    json["schemaVersion"] = theme.schemaVersion;
    json["id"] = theme.id;
    json["name"] = theme.name;
    json["displayName"] = theme.displayName;
    json["variant"] = std::string{ ToString(theme.variant) };

    Json::Value source{ Json::objectValue };
    source["type"] = std::string{ ToString(theme.source.type) };
    source["project"] = theme.source.project;
    source["author"] = theme.source.author;
    source["homepage"] = includeAttribution && theme.source.homepage ? Json::Value{ *theme.source.homepage } : Json::Value::nullSingleton();
    source["license"] = theme.source.license;
    if (includeAttribution && theme.source.revision)
    {
        source["revision"] = *theme.source.revision;
    }
    if (includeAttribution && theme.source.sourceFile)
    {
        source["sourceFile"] = *theme.source.sourceFile;
    }
    json["source"] = std::move(source);

    Json::Value terminal{ Json::objectValue };
    terminal["foreground"] = theme.terminal.foreground;
    terminal["background"] = theme.terminal.background;
    terminal["cursorColor"] = theme.terminal.cursorColor;
    terminal["cursorTextColor"] = theme.terminal.cursorTextColor;
    terminal["selectionBackground"] = theme.terminal.selectionBackground;
    terminal["selectionForeground"] = theme.terminal.selectionForeground;
    terminal["ansi"] = StringArray(theme.terminal.ansi);
    terminal["bright"] = StringArray(theme.terminal.bright);
    json["terminal"] = std::move(terminal);

    Json::Value window{ Json::objectValue };
    window["opacity"] = theme.window.opacity;
    window["useAcrylic"] = theme.window.useAcrylic;
    window["tabBarBackground"] = theme.window.tabBarBackground;
    window["inactiveTabBackground"] = theme.window.inactiveTabBackground;
    window["paneBorderColor"] = theme.window.paneBorderColor;
    json["window"] = std::move(window);

    if (theme.originalFileName)
    {
        json["originalFileName"] = LeafName(*theme.originalFileName);
    }
    return json;
}

std::string ThemeSerializer::Serialize(const ThemeDescriptor& theme, const bool includeAttribution)
{
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["emitUTF8"] = true;
    builder["indentation"] = "  ";
    builder["precision"] = 17;
    return Json::writeString(builder, ToJson(theme, includeAttribution)) + "\n";
}

std::string ThemeSerializer::ReadFile(const std::filesystem::path& path, const size_t maximumSize)
{
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error)
    {
        throw std::runtime_error("The selected theme file could not be read.");
    }
    if (size > maximumSize)
    {
        throw std::runtime_error("The imported file exceeds the maximum allowed size.");
    }

    std::ifstream input{ path, std::ios::binary };
    if (!input)
    {
        throw std::runtime_error("The selected theme file could not be read.");
    }
    std::string content(static_cast<size_t>(size), '\0');
    if (!content.empty())
    {
        input.read(content.data(), static_cast<std::streamsize>(content.size()));
        if (input.gcount() != static_cast<std::streamsize>(content.size()))
        {
            throw std::runtime_error("The selected theme file could not be read.");
        }
    }
    return content;
}

void ThemeSerializer::WriteFileAtomically(const std::filesystem::path& path, const std::string_view content)
{
    if (content.size() > MaximumThemeFileSize)
    {
        throw std::runtime_error("The exported theme exceeds the maximum allowed size.");
    }
    const auto parent = path.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent);
    }

    til::io::write_utf8_string_to_file_atomic(path, content);
}
