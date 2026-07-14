// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "FontRegistry.h"

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace winTerm::Appearance
{
    // A plain-data projection of results already produced by the existing
    // DirectWrite font enumeration path. This layer never performs another
    // system scan.
    struct SystemFontDiscoveryRecord
    {
        std::string id;
        std::string familyName;
        std::string fullName;
        std::optional<bool> monospaced;
        bool recommended{ false };
        GlyphCoverageEstimate nerdFontCoverage{ GlyphCoverageEstimate::Unknown };
        GlyphCoverageEstimate powerlineCoverage{ GlyphCoverageEstimate::Unknown };
        GlyphCoverageEstimate boxDrawingCoverage{ GlyphCoverageEstimate::Unknown };
        GlyphCoverageEstimate emojiCoverage{ GlyphCoverageEstimate::Unknown };
        std::vector<VariableFontAxis> variableAxes;
        std::string version;
    };

    class FontDiscovery final
    {
    public:
        static std::vector<FontDescriptor> AdaptSystemSnapshot(std::span<const SystemFontDiscoveryRecord> records);
        static FontSystemMergeResult MergeSystemSnapshot(FontRegistry& registry,
                                                         std::span<const SystemFontDiscoveryRecord> records);
    };
}
