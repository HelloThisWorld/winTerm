// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "FontDescriptor.h"

using namespace winTerm::Appearance;

bool FontDescriptor::IsBundled() const noexcept
{
    return source == FontSourceType::Bundled;
}

bool FontDescriptor::IsSystem() const noexcept
{
    return source == FontSourceType::System;
}

std::string_view winTerm::Appearance::ToString(const FontSourceType value) noexcept
{
    switch (value)
    {
    case FontSourceType::Bundled:
        return "bundled";
    case FontSourceType::System:
        return "system";
    }
    return "system";
}

std::string_view winTerm::Appearance::ToString(const FontPropertyState value) noexcept
{
    switch (value)
    {
    case FontPropertyState::Unknown:
        return "unknown";
    case FontPropertyState::No:
        return "no";
    case FontPropertyState::Yes:
        return "yes";
    }
    return "unknown";
}

std::string_view winTerm::Appearance::ToString(const GlyphCoverageEstimate value) noexcept
{
    switch (value)
    {
    case GlyphCoverageEstimate::Unknown:
        return "unknown";
    case GlyphCoverageEstimate::NotPresentInSample:
        return "notPresentInSample";
    case GlyphCoverageEstimate::Partial:
        return "partial";
    case GlyphCoverageEstimate::PresentInSample:
        return "presentInSample";
    }
    return "unknown";
}

std::optional<FontPropertyState> winTerm::Appearance::FontPropertyStateFromString(const std::string_view value) noexcept
{
    if (value == "unknown")
    {
        return FontPropertyState::Unknown;
    }
    if (value == "no" || value == "false" || value == "unsupported" || value == "absent")
    {
        return FontPropertyState::No;
    }
    if (value == "yes" || value == "true" || value == "supported" || value == "present")
    {
        return FontPropertyState::Yes;
    }
    return std::nullopt;
}

std::optional<GlyphCoverageEstimate> winTerm::Appearance::GlyphCoverageEstimateFromString(const std::string_view value) noexcept
{
    if (value == "unknown")
    {
        return GlyphCoverageEstimate::Unknown;
    }
    if (value == "notPresentInSample" || value == "notpresentinsample" ||
        value == "none" || value == "no" || value == "false" || value == "unsupported")
    {
        return GlyphCoverageEstimate::NotPresentInSample;
    }
    if (value == "partial")
    {
        return GlyphCoverageEstimate::Partial;
    }
    if (value == "presentInSample" || value == "presentinsample" ||
        value == "sampled" || value == "yes" || value == "true" || value == "supported")
    {
        return GlyphCoverageEstimate::PresentInSample;
    }
    return std::nullopt;
}
