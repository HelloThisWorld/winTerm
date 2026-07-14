// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "GlyphCoverageAnalyzer.h"

using namespace winTerm::Appearance;

namespace
{
    constexpr std::array<char32_t, 4> AsciiSamples{ U'A', U'a', U'0', U'~' };
    constexpr std::array<char32_t, 4> LatinSamples{ U'\u00C9', U'\u00DF', U'\u0142', U'\u0218' };
    constexpr std::array<char32_t, 3> CyrillicSamples{ U'\u0416', U'\u044F', U'\u0401' };
    constexpr std::array<char32_t, 3> HebrewSamples{ U'\u05D0', U'\u05DE', U'\u05EA' };
    constexpr std::array<char32_t, 3> ArabicSamples{ U'\u0627', U'\u0645', U'\u064A' };
    constexpr std::array<char32_t, 4> CjkSamples{ U'\u4E2D', U'\u6587', U'\u65E5', U'\uD55C' };
    constexpr std::array<char32_t, 6> BoxDrawingSamples{ U'\u250C', U'\u2500', U'\u2510', U'\u251C', U'\u253C', U'\u2518' };
    constexpr std::array<char32_t, 4> BlockSamples{ U'\u2588', U'\u2591', U'\u2592', U'\u2593' };
    constexpr std::array<char32_t, 4> BrailleSamples{ U'\u280B', U'\u2819', U'\u2838', U'\u28FF' };
    constexpr std::array<char32_t, 5> MathematicalSamples{ U'\u2264', U'\u2265', U'\u2260', U'\u00B1', U'\u221E' };
    constexpr std::array<char32_t, 4> PowerlineSamples{ U'\uE0B0', U'\uE0B1', U'\uE0B2', U'\uE0B3' };
    constexpr std::array<char32_t, 4> NerdFontSamples{ U'\uE5FA', U'\uF17C', U'\uF120', U'\uF489' };
    constexpr std::array<char32_t, 5> EmojiSamples{ U'\U0001F600', U'\U0001F680', U'\U0001F525', U'\u2615', U'\u2699' };
    constexpr std::array<char32_t, 0> NoSamples{};

    constexpr bool InRange(const char32_t value, const char32_t first, const char32_t last) noexcept
    {
        return value >= first && value <= last;
    }

    GlyphCoverageEstimate Estimate(const GlyphCategoryCoverage& coverage) noexcept
    {
        if (coverage.sampleCount == 0 || coverage.knownCount == 0)
        {
            return GlyphCoverageEstimate::Unknown;
        }
        if (coverage.presentCount == 0)
        {
            return GlyphCoverageEstimate::NotPresentInSample;
        }
        if (coverage.knownCount != coverage.sampleCount || coverage.presentCount != coverage.knownCount)
        {
            return GlyphCoverageEstimate::Partial;
        }
        return GlyphCoverageEstimate::PresentInSample;
    }
}

const GlyphCategoryCoverage& GlyphCoverageReport::For(const GlyphCategory category) const noexcept
{
    const auto index = static_cast<size_t>(category);
    return categories[index < categories.size() ? index : static_cast<size_t>(GlyphCategory::Other)];
}

GlyphCoverageReport GlyphCoverageAnalyzer::Analyze(const std::span<const GlyphObservation> observations) const
{
    GlyphCoverageReport report;
    for (size_t i = 0; i < report.categories.size(); ++i)
    {
        report.categories[i].category = static_cast<GlyphCategory>(i);
    }

    for (const auto& observation : observations)
    {
        auto& coverage = report.categories[static_cast<size_t>(ClassifyCodePoint(observation.codePoint))];
        ++coverage.sampleCount;
        if (observation.present)
        {
            ++coverage.knownCount;
            if (*observation.present)
            {
                ++coverage.presentCount;
            }
        }
    }

    for (auto& coverage : report.categories)
    {
        coverage.estimate = Estimate(coverage);
    }
    return report;
}

GlyphCategory GlyphCoverageAnalyzer::ClassifyCodePoint(const char32_t codePoint) noexcept
{
    if (InRange(codePoint, U'\uE0A0', U'\uE0D7'))
    {
        return GlyphCategory::Powerline;
    }
    if (InRange(codePoint, U'\uE000', U'\uF8FF') ||
        InRange(codePoint, U'\U000F0000', U'\U000FFFFD') ||
        InRange(codePoint, U'\U00100000', U'\U0010FFFD'))
    {
        return GlyphCategory::NerdFontPrivateUse;
    }
    if (InRange(codePoint, U'\U0001F000', U'\U0001FAFF') ||
        InRange(codePoint, U'\u2600', U'\u26FF') ||
        InRange(codePoint, U'\u2700', U'\u27BF'))
    {
        return GlyphCategory::Emoji;
    }
    if (InRange(codePoint, U'\u0020', U'\u007E'))
    {
        return GlyphCategory::Ascii;
    }
    if (InRange(codePoint, U'\u00A0', U'\u024F') || InRange(codePoint, U'\u1E00', U'\u1EFF'))
    {
        return GlyphCategory::Latin;
    }
    if (InRange(codePoint, U'\u0400', U'\u052F'))
    {
        return GlyphCategory::Cyrillic;
    }
    if (InRange(codePoint, U'\u0590', U'\u05FF'))
    {
        return GlyphCategory::Hebrew;
    }
    if (InRange(codePoint, U'\u0600', U'\u06FF') ||
        InRange(codePoint, U'\u0750', U'\u077F') ||
        InRange(codePoint, U'\u08A0', U'\u08FF'))
    {
        return GlyphCategory::Arabic;
    }
    if (InRange(codePoint, U'\u2E80', U'\u9FFF') ||
        InRange(codePoint, U'\uAC00', U'\uD7AF') ||
        InRange(codePoint, U'\U00020000', U'\U000323AF'))
    {
        return GlyphCategory::Cjk;
    }
    if (InRange(codePoint, U'\u2500', U'\u257F'))
    {
        return GlyphCategory::BoxDrawing;
    }
    if (InRange(codePoint, U'\u2580', U'\u259F'))
    {
        return GlyphCategory::BlockElements;
    }
    if (InRange(codePoint, U'\u2800', U'\u28FF'))
    {
        return GlyphCategory::Braille;
    }
    if (InRange(codePoint, U'\u2200', U'\u22FF') || InRange(codePoint, U'\u27C0', U'\u27EF'))
    {
        return GlyphCategory::MathematicalSymbols;
    }
    return GlyphCategory::Other;
}

std::span<const char32_t> GlyphCoverageAnalyzer::RepresentativeSamples(const GlyphCategory category) noexcept
{
    switch (category)
    {
    case GlyphCategory::Ascii:
        return AsciiSamples;
    case GlyphCategory::Latin:
        return LatinSamples;
    case GlyphCategory::Cyrillic:
        return CyrillicSamples;
    case GlyphCategory::Hebrew:
        return HebrewSamples;
    case GlyphCategory::Arabic:
        return ArabicSamples;
    case GlyphCategory::Cjk:
        return CjkSamples;
    case GlyphCategory::BoxDrawing:
        return BoxDrawingSamples;
    case GlyphCategory::BlockElements:
        return BlockSamples;
    case GlyphCategory::Braille:
        return BrailleSamples;
    case GlyphCategory::MathematicalSymbols:
        return MathematicalSamples;
    case GlyphCategory::Powerline:
        return PowerlineSamples;
    case GlyphCategory::NerdFontPrivateUse:
        return NerdFontSamples;
    case GlyphCategory::Emoji:
        return EmojiSamples;
    case GlyphCategory::Other:
    case GlyphCategory::Count:
        return NoSamples;
    }
    return NoSamples;
}

void GlyphCoverageAnalyzer::ApplyEstimates(const GlyphCoverageReport& report, FontDescriptor& descriptor) noexcept
{
    descriptor.nerdFontCoverage = report.For(GlyphCategory::NerdFontPrivateUse).estimate;
    descriptor.powerlineCoverage = report.For(GlyphCategory::Powerline).estimate;
    descriptor.boxDrawingCoverage = report.For(GlyphCategory::BoxDrawing).estimate;
    descriptor.emojiCoverage = report.For(GlyphCategory::Emoji).estimate;
}

std::string_view winTerm::Appearance::ToString(const GlyphCategory value) noexcept
{
    switch (value)
    {
    case GlyphCategory::Ascii:
        return "ascii";
    case GlyphCategory::Latin:
        return "latin";
    case GlyphCategory::Cyrillic:
        return "cyrillic";
    case GlyphCategory::Hebrew:
        return "hebrew";
    case GlyphCategory::Arabic:
        return "arabic";
    case GlyphCategory::Cjk:
        return "cjk";
    case GlyphCategory::BoxDrawing:
        return "boxDrawing";
    case GlyphCategory::BlockElements:
        return "blockElements";
    case GlyphCategory::Braille:
        return "braille";
    case GlyphCategory::MathematicalSymbols:
        return "mathematicalSymbols";
    case GlyphCategory::Powerline:
        return "powerline";
    case GlyphCategory::NerdFontPrivateUse:
        return "nerdFontPrivateUse";
    case GlyphCategory::Emoji:
        return "emoji";
    case GlyphCategory::Other:
        return "other";
    case GlyphCategory::Count:
        break;
    }
    return "other";
}
