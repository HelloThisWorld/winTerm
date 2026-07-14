// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "FontRegistry.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace winTerm::Appearance
{
    struct FontFallbackOptions
    {
        bool fallbackEnabled{ true };
        bool emojiFallbackEnabled{ true };
    };

    class FontFallbackResolver final
    {
    public:
        static std::vector<std::string> Resolve(const FontRegistry& registry,
                                                std::string_view selectedFamily,
                                                FontFallbackOptions options = {});
        static std::vector<std::string> Resolve(std::span<const std::string> availableFamilies,
                                                std::string_view selectedFamily,
                                                FontFallbackOptions options = {});
    };
}
