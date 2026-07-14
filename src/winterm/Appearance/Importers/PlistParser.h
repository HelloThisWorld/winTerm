// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Themes/ThemeDescriptor.h"

namespace winTerm::Appearance
{
    class PlistParser final
    {
    public:
        static Json::Value Parse(std::string_view content);
    };
}
