// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace winTerm::Appearance
{
    enum class FontSourceType
    {
        Bundled,
        System,
    };

    // This state is intentionally tri-state. Discovery must not infer a value
    // when the font metrics could not be inspected reliably.
    enum class FontPropertyState
    {
        Unknown,
        No,
        Yes,
    };

    // Coverage is based on representative glyph observations, not a guarantee
    // that every code point or shaped sequence in the category is supported.
    enum class GlyphCoverageEstimate
    {
        Unknown,
        NotPresentInSample,
        Partial,
        PresentInSample,
    };

    struct VariableFontAxis
    {
        std::string tag;
        std::string name;
        double minimum{};
        double maximum{};
        double defaultValue{};
    };

    struct FontDescriptor
    {
        std::string id;
        std::string familyName;
        std::string fullName;
        FontSourceType source{ FontSourceType::System };
        bool recommended{ false };
        FontPropertyState monospaced{ FontPropertyState::Unknown };
        GlyphCoverageEstimate nerdFontCoverage{ GlyphCoverageEstimate::Unknown };
        GlyphCoverageEstimate powerlineCoverage{ GlyphCoverageEstimate::Unknown };
        GlyphCoverageEstimate boxDrawingCoverage{ GlyphCoverageEstimate::Unknown };
        GlyphCoverageEstimate emojiCoverage{ GlyphCoverageEstimate::Unknown };
        std::vector<VariableFontAxis> variableAxes;
        std::string version;
        std::string author;
        std::string license;
        std::string sourceProject;
        std::string sourceRevision;
        std::optional<std::filesystem::path> file;
        std::optional<std::filesystem::path> licenseFile;
        std::string sha256;

        bool IsBundled() const noexcept;
        bool IsSystem() const noexcept;
    };

    std::string_view ToString(FontSourceType value) noexcept;
    std::string_view ToString(FontPropertyState value) noexcept;
    std::string_view ToString(GlyphCoverageEstimate value) noexcept;
    std::optional<FontPropertyState> FontPropertyStateFromString(std::string_view value) noexcept;
    std::optional<GlyphCoverageEstimate> GlyphCoverageEstimateFromString(std::string_view value) noexcept;
}
