// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <string_view>

namespace winTerm::Control::SearchUx
{
    // The search box drops secondary controls as the pane narrows. The
    // thresholds compare against the pane width in DIPs and are derived from
    // the measured widths of the three layouts, so each state fits within the
    // range that selects it.
    enum class SearchBoxLayoutState
    {
        Normal,
        Compact,
        Minimal,
    };

    inline constexpr double SearchBoxCompactLayoutThreshold{ 440.0 };
    inline constexpr double SearchBoxMinimalLayoutThreshold{ 330.0 };

    constexpr SearchBoxLayoutState SearchBoxLayoutStateForWidth(const double availableWidth) noexcept
    {
        // A non-positive width means the host has not been measured yet.
        if (availableWidth <= 0.0)
        {
            return SearchBoxLayoutState::Normal;
        }
        if (availableWidth < SearchBoxMinimalLayoutThreshold)
        {
            return SearchBoxLayoutState::Minimal;
        }
        if (availableWidth < SearchBoxCompactLayoutThreshold)
        {
            return SearchBoxLayoutState::Compact;
        }
        return SearchBoxLayoutState::Normal;
    }

    constexpr std::wstring_view SearchBoxLayoutStateName(const SearchBoxLayoutState state) noexcept
    {
        switch (state)
        {
        case SearchBoxLayoutState::Minimal:
            return L"MinimalLayout";
        case SearchBoxLayoutState::Compact:
            return L"CompactLayout";
        default:
            return L"NormalLayout";
        }
    }

    // The scrollbar mark surface renders for generic marks (gated by the
    // ShowMarks setting) and for search overview pips (gated by an open
    // search), each category keeping its own rule.
    constexpr bool ShouldRenderScrollbarMarkSurface(const bool genericMarksEnabled, const bool searchActive) noexcept
    {
        return genericMarksEnabled || searchActive;
    }

    // Enumerates the distinct buffer rows of a search result collection in
    // order. The scrollbar overview draws one pip per row no matter how many
    // occurrences the row contains; the search box keeps reporting
    // occurrences.
    template<typename SpanCollection, typename RowFunc>
    constexpr void ForEachDistinctSearchRow(const SpanCollection& spans, RowFunc&& fn)
    {
        bool first = true;
        decltype(std::begin(spans)->start.y) lastRow{};
        for (const auto& span : spans)
        {
            if (first || span.start.y != lastRow)
            {
                first = false;
                lastRow = span.start.y;
                fn(lastRow);
            }
        }
    }
}
