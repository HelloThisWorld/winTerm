// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace winTerm::Appearance
{
    inline constexpr double MinimumFontSize{ 1.0 };
    inline constexpr double MaximumFontSize{ 256.0 };
    inline constexpr uint16_t MinimumFontWeight{ 1 };
    inline constexpr uint16_t MaximumFontWeight{ 1000 };
    inline constexpr double MinimumLineHeight{ 0.5 };
    inline constexpr double MaximumLineHeight{ 4.0 };
    inline constexpr double MinimumCellWidthAdjustment{ -0.5 };
    inline constexpr double MaximumCellWidthAdjustment{ 1.0 };

    struct FontFeatureValidationIssue
    {
        std::string field;
        std::string message;
    };

    struct FontFeatureValidationResult
    {
        std::vector<FontFeatureValidationIssue> issues;

        bool IsValid() const noexcept;
        std::vector<std::string> ErrorMessages() const;
    };

    struct FontFeatureSettings
    {
        double size{ 12.0 };
        uint16_t weight{ 400 };
        double lineHeight{ 1.0 };
        double cellWidthAdjustment{};
        bool ligatures{ true };
        bool contextualAlternates{ true };
        bool fallbackEnabled{ true };
        bool emojiFallbackEnabled{ true };

        FontFeatureValidationResult Validate() const;
    };
}
