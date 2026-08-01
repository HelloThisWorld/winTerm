// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace winTerm::VisualProgress::RainbowArcVisualConstants
{
    // Geometry is expressed in XAML effective pixels (DIPs). The composition
    // child visual inherits XAML's rasterization scale, so these values remain
    // correct at 100%, 125%, 150%, and 200% display scaling.
    inline constexpr float TrackHeight{ 6.0f };
    inline constexpr float HorizontalInset{ 10.0f };
    inline constexpr float BottomInset{ 8.0f };
    inline constexpr float TrackCornerRadius{ TrackHeight / 2.0f };
    inline constexpr float MinimumDrawableTrackWidth{ 2.0f };

    inline constexpr float WhiteCoreWidth{ 2.5f };
    inline constexpr float WarmCoreWidth{ 5.0f };
    inline constexpr float HeadTrailWidth{ 8.0f };
    inline constexpr float InnerGlowWidth{ 12.0f };
    inline constexpr float InnerGlowHeight{ 10.0f };
    inline constexpr float OuterBloomWidth{ 26.0f };
    inline constexpr float OuterBloomHeight{ 16.0f };
    // Reserves the track, its bottom inset, and the full outer bloom without
    // making Pane.cpp duplicate renderer geometry knowledge.
    inline constexpr float OverlayHostHeight{ BottomInset + TrackHeight + OuterBloomHeight };
    inline constexpr float SuccessSweepWidthFraction{ 0.16f };
    inline constexpr float IndeterminateTailFraction{ 0.25f };

    inline constexpr float TypicalSparkSize{ 1.5f };
    inline constexpr float MinimumSparkSize{ 1.0f };
    inline constexpr float MaximumSparkSize{ 3.0f };
    inline constexpr float MinimumSparkTravel{ 6.0f };
    inline constexpr float MaximumSparkTravel{ 18.0f };
    inline constexpr float MaximumSparkVerticalTravel{ 7.0f };
    inline constexpr float SparkBandHeight{ 22.0f };

    // All animation periods are compositor-managed. They are centralized here
    // to keep the tuning surface out of Pane.cpp and out of terminal-core code.
    inline constexpr std::chrono::milliseconds RainbowCycleDuration{ 2000 };
    inline constexpr std::chrono::milliseconds IndeterminateCycleDuration{ 1800 };
    inline constexpr std::chrono::milliseconds DeterminateInterpolationDuration{ 220 };
    inline constexpr std::chrono::milliseconds RegressionInterpolationDuration{ 240 };
    inline constexpr std::chrono::milliseconds RegressionOpacityDipDuration{ 90 };
    inline constexpr std::chrono::milliseconds WaitingBreatheDuration{ 1600 };
    inline constexpr std::chrono::milliseconds SuccessAdvanceDuration{ 220 };
    inline constexpr std::chrono::milliseconds SuccessIntensifyDuration{ 140 };
    inline constexpr std::chrono::milliseconds SuccessSweepDuration{ 320 };
    inline constexpr std::chrono::milliseconds SuccessPresentationDuration{ 480 };
    inline constexpr std::chrono::milliseconds SuccessFadeDuration{ 650 };
    inline constexpr std::chrono::milliseconds ErrorPulseDuration{ 220 };
    inline constexpr std::chrono::milliseconds CancelFadeDuration{ 180 };
    inline constexpr std::chrono::milliseconds MinimumSparkLifetime{ 120 };
    inline constexpr std::chrono::milliseconds MaximumSparkLifetime{ 260 };
    inline constexpr std::chrono::milliseconds AmbientSparkCycleOne{ 1450 };
    inline constexpr std::chrono::milliseconds AmbientSparkCycleTwo{ 1950 };
    inline constexpr std::chrono::milliseconds AmbientSparkStagger{ 370 };

    // Fixed renderer and recognition bounds. These are capacities, not tuning
    // suggestions: callers must fail open rather than grow them dynamically.
    inline constexpr std::size_t RainbowStopCount{ 9 };
    inline constexpr std::size_t SparkPoolCapacityPerPane{ 8 };
    inline constexpr std::size_t SparkCapacityPerWindowOrProcess{ 24 };
    inline constexpr std::size_t NormalSparkBurstMinimum{ 1 };
    inline constexpr std::size_t NormalSparkBurstMaximum{ 2 };
    inline constexpr std::size_t StrongSparkBurstMinimum{ 3 };
    inline constexpr std::size_t StrongSparkBurstMaximum{ 6 };
    inline constexpr std::size_t AmbientSparkSlotCount{ 2 };

    // ARGB palette. High Contrast colors are resolved from UISettings at run
    // time and therefore deliberately do not live in this fixed palette.
    inline constexpr uint32_t DarkTrack{ 0x99111820u };
    inline constexpr uint32_t LightTrack{ 0x665F6F7Cu };
    inline constexpr uint32_t RunningSolid{ 0xFF63E6BEu };
    inline constexpr uint32_t WaitingSolid{ 0xFFF4C95Du };
    inline constexpr uint32_t SuccessSolid{ 0xFF4DDC88u };
    inline constexpr uint32_t ErrorSolid{ 0xFFFF6B6Bu };
    // Light-surface status colors mirror the app theme resources in App.xaml.
    // The brighter dark-surface palette does not provide sufficient contrast
    // for Reduced Motion and other solid fallbacks on a light background.
    inline constexpr uint32_t LightRunningSolid{ 0xFF087A63u };
    inline constexpr uint32_t LightWaitingSolid{ 0xFF8A5D00u };
    inline constexpr uint32_t LightSuccessSolid{ 0xFF087A42u };
    inline constexpr uint32_t LightErrorSolid{ 0xFFB42318u };
    inline constexpr uint32_t WarmWhite{ 0xFFFFF2B3u };
    inline constexpr uint32_t WhiteHot{ 0xFFFFFFFFu };
    inline constexpr uint32_t SparkYellow{ 0xFFFFD166u };
    inline constexpr uint32_t SparkOrange{ 0xFFFF8A3Du };

    inline constexpr uint32_t RainbowRed{ 0xFFFF3B30u };
    inline constexpr uint32_t RainbowOrange{ 0xFFFF8A00u };
    inline constexpr uint32_t RainbowYellow{ 0xFFFFD60Au };
    inline constexpr uint32_t RainbowGreen{ 0xFF34C759u };
    inline constexpr uint32_t RainbowCyan{ 0xFF32D6E9u };
    inline constexpr uint32_t RainbowBlue{ 0xFF0A84FFu };
    inline constexpr uint32_t RainbowViolet{ 0xFF7D5CFFu };
    inline constexpr uint32_t RainbowMagenta{ 0xFFFF2D95u };
}
