// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ThemeLoader.h"
#include "ThemeSerializer.h"
#include "../Importers/AppleTerminalImporter.h"
#include "../Importers/ITermColorsImporter.h"
#include "../Importers/WindowsTerminalSchemeImporter.h"
#include "../Importers/WinTermThemeImporter.h"

using namespace winTerm::Appearance;

namespace
{
    bool EndsWithInsensitive(const std::string_view value, const std::string_view suffix)
    {
        return value.size() >= suffix.size() &&
               std::equal(suffix.begin(), suffix.end(), value.end() - static_cast<std::ptrdiff_t>(suffix.size()), [](const char left, const char right) {
                   return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
               });
    }
}

ThemeImportResult ThemeLoader::Import(const std::string_view content, const std::string_view fileName)
{
    if (EndsWithInsensitive(fileName, ".itermcolors"))
    {
        return ITermColorsImporter{}.Import(content, fileName);
    }
    if (EndsWithInsensitive(fileName, ".terminal"))
    {
        return AppleTerminalImporter{}.Import(content, fileName);
    }
    if (EndsWithInsensitive(fileName, ".winterm-theme.json"))
    {
        return WinTermThemeImporter{}.Import(content, fileName);
    }
    if (EndsWithInsensitive(fileName, ".json"))
    {
        const auto document = ThemeSerializer::ParseJsonDocument(content);
        if (document.isObject() && document.isMember("schemaVersion") && document.isMember("terminal"))
        {
            return WinTermThemeImporter{}.Import(content, fileName);
        }
        return WindowsTerminalSchemeImporter{}.Import(content, fileName);
    }
    throw std::runtime_error("This theme format is not supported.");
}

ThemeImportResult ThemeLoader::ImportFile(const std::filesystem::path& path)
{
    const auto content = ThemeSerializer::ReadFile(path);
    return Import(content, til::u16u8(path.filename().wstring()));
}

ThemeDescriptor ThemeLoader::LoadWinTermThemeFile(const std::filesystem::path& path)
{
    return ThemeSerializer::Deserialize(ThemeSerializer::ReadFile(path));
}

void ThemeLoader::ExportFile(const ThemeDescriptor& theme, const std::filesystem::path& path, const bool includeAttribution)
{
    ThemeSerializer::WriteFileAtomically(path, ThemeSerializer::Serialize(theme, includeAttribution));
}
