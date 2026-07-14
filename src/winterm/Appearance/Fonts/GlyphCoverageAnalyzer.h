// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "FontDescriptor.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace winTerm::Appearance
{
    enum class GlyphCategory
    {
        Ascii,
        Latin,
        Cyrillic,
        Hebrew,
        Arabic,
        Cjk,
        BoxDrawing,
        BlockElements,
        Braille,
        MathematicalSymbols,
        Powerline,
        NerdFontPrivateUse,
        Emoji,
        Other,
        Count,
    };

    struct GlyphObservation
    {
        char32_t codePoint{};
        // nullopt means that the discovery backend could not determine whether
        // the glyph is present.
        std::optional<bool> present;
    };

    struct GlyphCategoryCoverage
    {
        GlyphCategory category{ GlyphCategory::Other };
        size_t sampleCount{};
        size_t knownCount{};
        size_t presentCount{};
        GlyphCoverageEstimate estimate{ GlyphCoverageEstimate::Unknown };
    };

    struct GlyphCoverageReport
    {
        std::array<GlyphCategoryCoverage, static_cast<size_t>(GlyphCategory::Count)> categories;

        const GlyphCategoryCoverage& For(GlyphCategory category) const noexcept;
    };

    class IGlyphCoverageAnalyzer
    {
    public:
        virtual ~IGlyphCoverageAnalyzer() = default;
        virtual GlyphCoverageReport Analyze(std::span<const GlyphObservation> observations) const = 0;
    };

    // This implementation only classifies observations supplied by a discovery
    // backend. It does not shape text, inspect the renderer, or claim exhaustive
    // Unicode coverage.
    class GlyphCoverageAnalyzer final : public IGlyphCoverageAnalyzer
    {
    public:
        GlyphCoverageReport Analyze(std::span<const GlyphObservation> observations) const override;

        static GlyphCategory ClassifyCodePoint(char32_t codePoint) noexcept;
        static std::span<const char32_t> RepresentativeSamples(GlyphCategory category) noexcept;
        static void ApplyEstimates(const GlyphCoverageReport& report, FontDescriptor& descriptor) noexcept;
    };

    std::string_view ToString(GlyphCategory value) noexcept;
}
