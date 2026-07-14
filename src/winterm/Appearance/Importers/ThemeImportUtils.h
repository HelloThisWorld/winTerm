// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Themes/ThemeDescriptor.h"

namespace winTerm::Appearance::details
{
    TerminalTheme DefaultTerminalTheme();
    std::optional<std::string> DecodePlistColor(const Json::Value& value);
    ThemeVariant VariantFromBackground(std::string_view background) noexcept;
}
