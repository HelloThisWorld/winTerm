// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "FontDiscovery.h"

#include <utility>

using namespace winTerm::Appearance;

std::vector<FontDescriptor> FontDiscovery::AdaptSystemSnapshot(const std::span<const SystemFontDiscoveryRecord> records)
{
    std::vector<FontDescriptor> descriptors;
    descriptors.reserve(records.size());
    for (const auto& record : records)
    {
        FontDescriptor descriptor;
        descriptor.id = record.id;
        descriptor.familyName = record.familyName;
        descriptor.fullName = record.fullName.empty() ? record.familyName : record.fullName;
        descriptor.source = FontSourceType::System;
        descriptor.recommended = record.recommended;
        descriptor.monospaced = record.monospaced ? (*record.monospaced ? FontPropertyState::Yes : FontPropertyState::No)
                                                  : FontPropertyState::Unknown;
        descriptor.nerdFontCoverage = record.nerdFontCoverage;
        descriptor.powerlineCoverage = record.powerlineCoverage;
        descriptor.boxDrawingCoverage = record.boxDrawingCoverage;
        descriptor.emojiCoverage = record.emojiCoverage;
        descriptor.variableAxes = record.variableAxes;
        descriptor.version = record.version;
        descriptors.emplace_back(std::move(descriptor));
    }
    return descriptors;
}

FontSystemMergeResult FontDiscovery::MergeSystemSnapshot(FontRegistry& registry,
                                                         const std::span<const SystemFontDiscoveryRecord> records)
{
    const auto descriptors = AdaptSystemSnapshot(records);
    return registry.MergeSystemFonts(std::span<const FontDescriptor>{ descriptors });
}
