// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "WinTermThemeImporter.h"
#include "../Themes/ThemeSerializer.h"
#include "../Themes/ThemeValidator.h"

using namespace winTerm::Appearance;

ThemeImportResult WinTermThemeImporter::Import(const std::string_view content, const std::string_view fileName) const
{
    auto theme = ThemeSerializer::Deserialize(content);
    theme.source.type = ThemeSourceType::Imported;
    theme.originalFileName = LeafName(fileName);

    const auto validation = ThemeValidator::Validate(theme);
    if (!validation.IsValid())
    {
        throw std::runtime_error("The selected file is not a valid winTerm theme.");
    }

    ThemeImportResult result;
    result.summary.imported = { "Terminal color palette", "Window appearance", "Theme attribution" };
    result.themes.emplace_back(std::move(theme));
    return result;
}
