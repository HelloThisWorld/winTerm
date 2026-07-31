// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <cstdint>

namespace winTerm::Design::ColorTokens
{
    // ARGB values follow the approved winTerm website product visuals.
    inline constexpr uint32_t AppBackground{ 0xFF070A0F };
    inline constexpr uint32_t WindowSurface{ 0xFF090E14 };
    inline constexpr uint32_t TitlebarTop{ 0xFF17212B };
    inline constexpr uint32_t TitlebarBottom{ 0xFF131C25 };
    inline constexpr uint32_t TitlebarBorder{ 0xFF24313C };
    inline constexpr uint32_t TabSurface{ 0xFF111820 };
    inline constexpr uint32_t ActiveTabSurface{ 0xFF1A2530 };
    inline constexpr uint32_t TabBorder{ 0xFF1D2A35 };
    inline constexpr uint32_t PaneHeaderSurface{ 0xFF101821 };
    inline constexpr uint32_t PaneHeaderBorder{ 0xFF1F2D38 };
    inline constexpr uint32_t TerminalSurface{ 0xFF0A1016 };
    inline constexpr uint32_t BorderIdle{ 0xFF2A3945 };
    inline constexpr uint32_t BorderHover{ 0xFF34485A };
    inline constexpr uint32_t AccentMint{ 0xFF63E6BE };
    inline constexpr uint32_t AccentMintStrong{ 0xFF41D8AA };
    inline constexpr uint32_t TextPrimary{ 0xFFF2F7FB };
    inline constexpr uint32_t TextSecondary{ 0xFFC5D0DA };
    inline constexpr uint32_t TextMuted{ 0xFF8E9EAD };
    inline constexpr uint32_t IndicatorSurface{ 0xF2111B24 };
    inline constexpr uint32_t ProgressTrack{ 0x99111820 };
    inline constexpr uint32_t ProgressWaiting{ 0xFFF4C95D };
    inline constexpr uint32_t ProgressSuccess{ 0xFF4DDC88 };
    inline constexpr uint32_t ProgressError{ 0xFFFF6B6B };
}
