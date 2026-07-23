// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

namespace winTerm::Design
{
    enum class InterfaceDensity
    {
        Compact,
        Comfortable,
    };

    namespace DensityTokens
    {
        inline constexpr InterfaceDensity Default{ InterfaceDensity::Compact };
        inline constexpr double CompactPaneHeaderHeight{ 27.0 };
        inline constexpr double ComfortablePaneHeaderHeight{ 32.0 };
        inline constexpr double CompactTabHeight{ 32.0 };
        inline constexpr double CompactTitlebarHeight{ 35.0 };
    }
}
