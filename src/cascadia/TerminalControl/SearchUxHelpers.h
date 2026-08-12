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

    // Everything the scrollbar mark bitmap's content depends on. The bitmap
    // is repainted only when one of these inputs changes; a scroll update
    // that merely moves the thumb repaints nothing, which keeps scrolling
    // cheap with very large result sets. The search core stays the source of
    // truth — this is an identity of its state, not a copy of it.
    struct ScrollbarMarkPaintState
    {
        double maximum{ 0 };
        double viewportSize{ 0 };
        int32_t widthPx{ 0 };
        int32_t heightPx{ 0 };
        bool genericMarks{ false };
        bool searchMarks{ false };
        uint64_t searchGeneration{ 0 };
        uint64_t bufferMutationId{ 0 };
        uint32_t searchPipColor{ 0 };

        friend constexpr bool operator==(const ScrollbarMarkPaintState&, const ScrollbarMarkPaintState&) noexcept = default;
    };

    // Builds the paint state for one update. Inputs only participate while
    // the category that consumes them renders: the buffer mutation id backs
    // the generic marks (their content derives from buffer rows), and the pip
    // color backs the search overview (searchGeneration covers its rows).
    // Without this rule, an alt-screen application mutating the buffer would
    // force a repaint per throttle tick even though the overview cannot
    // change.
    constexpr ScrollbarMarkPaintState MakeScrollbarMarkPaintState(const double maximum,
                                                                  const double viewportSize,
                                                                  const int32_t widthPx,
                                                                  const int32_t heightPx,
                                                                  const bool genericMarks,
                                                                  const bool searchMarks,
                                                                  const uint64_t searchGeneration,
                                                                  const uint64_t bufferMutationId,
                                                                  const uint32_t searchPipColor) noexcept
    {
        return ScrollbarMarkPaintState{
            .maximum = maximum,
            .viewportSize = viewportSize,
            .widthPx = widthPx,
            .heightPx = heightPx,
            .genericMarks = genericMarks,
            .searchMarks = searchMarks,
            .searchGeneration = searchGeneration,
            .bufferMutationId = genericMarks ? bufferMutationId : 0,
            .searchPipColor = searchMarks ? searchPipColor : 0,
        };
    }

    // A repaint is needed when there is no previously painted state (first
    // paint, or the canvas was collapsed/invalidated) or when any input
    // changed. `hasPrevious`/`previous` mirror std::optional without forcing
    // the header to include it.
    constexpr bool ShouldRepaintScrollbarMarks(const bool hasPrevious,
                                               const ScrollbarMarkPaintState& previous,
                                               const ScrollbarMarkPaintState& next) noexcept
    {
        return !hasPrevious || !(previous == next);
    }
}
