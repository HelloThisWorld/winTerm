// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "MotionTokens.h"

#include <array>

namespace winTerm::Design::InteractionTokens
{
    inline constexpr double DividerVisibleThickness{ 1.0 };
    inline constexpr double DividerPointerHitThickness{ 12.0 };
    inline constexpr double SnapThreshold{ 8.0 };
    inline constexpr double SnapReleaseThreshold{ 14.0 };
    inline constexpr auto SnapIndicatorDuration{ MotionTokens::SnapIndicatorDuration };
    inline constexpr auto ResizeHoverDuration{ MotionTokens::ResizeHoverDuration };
    inline constexpr auto MenuTransitionDuration{ MotionTokens::MenuTransitionDuration };
    inline constexpr std::array<double, 5> CommonSnapRatios{
        0.25,
        1.0 / 3.0,
        0.5,
        2.0 / 3.0,
        0.75,
    };
}
