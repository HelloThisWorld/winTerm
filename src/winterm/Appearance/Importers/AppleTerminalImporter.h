// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "ThemeImporter.h"

namespace winTerm::Appearance
{
    class AppleTerminalImporter final : public ThemeImporter
    {
    public:
        ThemeImportResult Import(std::string_view content, std::string_view fileName) const override;
    };
}
