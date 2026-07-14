// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "ThemeDescriptor.h"

#include <span>

namespace winTerm::Appearance
{
    class ThemeValidator final
    {
    public:
        static ThemeValidationResult Validate(const ThemeDescriptor& theme);
        static ThemeValidationResult ValidateCollection(std::span<const ThemeDescriptor> themes);
        static bool TryNormalizeColor(std::string_view input, std::string& normalized);
        static bool IsValidId(std::string_view id) noexcept;
    };
}
