// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include "../../winterm/PaneResize/PaneResizeModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace WEX::TestExecution;
using namespace winTerm::PaneResize;

namespace
{
    constexpr ResizeGeometry StandardGeometry{
        1000.0,
        100.0,
        100.0,
    };

    bool Near(const double left, const double right)
    {
        return std::abs(left - right) < 0.0001;
    }
}

namespace SettingsModelUnitTests
{
    class WinTermPaneResizeTests
    {
        TEST_CLASS(WinTermPaneResizeTests);

        TEST_METHOD(CommonRatiosSnapAndReportExpectedLabels);
        TEST_METHOD(InvalidSnapRatiosAreFilteredByMinimumSizes);
        TEST_METHOD(AltBypassesSnapping);
        TEST_METHOD(HysteresisUsesLargerReleaseThreshold);
        TEST_METHOD(CancelRestoresOriginalRatio);
        TEST_METHOD(NestedResizeUsesOwningSplitGeometry);
        TEST_METHOD(LogicalThresholdIsStableAcrossDpiScales);
        TEST_METHOD(CustomRatioValidationRejectsUnsafeInput);
        TEST_METHOD(OneCommittedDragCreatesOneUndoEntry);
        TEST_METHOD(NewResizeClearsRedo);
        TEST_METHOD(FiveHundredResizesKeepHistoryBounded);
    };

    void WinTermPaneResizeTests::CommonRatiosSnapAndReportExpectedLabels()
    {
        const std::vector<std::pair<double, std::string>> cases{
            { 0.25, "1 / 4" },
            { 1.0 / 3.0, "1 / 3" },
            { 0.5, "50 / 50" },
            { 2.0 / 3.0, "2 / 3" },
            { 0.75, "3 / 4" },
        };
        uint64_t ownerId = 1;
        for (const auto& [ratio, label] : cases)
        {
            PaneResizeTransaction transaction;
            VERIFY_IS_TRUE(transaction.Begin(ownerId++, 0.42, StandardGeometry));
            const auto update = transaction.Update(ratio * StandardGeometry.containerLength + 3.0, false);
            VERIFY_IS_TRUE(update.snapped);
            VERIFY_IS_TRUE(Near(ratio, update.ratio));
            VERIFY_ARE_EQUAL(label, update.indicator);
            VERIFY_IS_TRUE(transaction.Commit().has_value());
        }
    }

    void WinTermPaneResizeTests::InvalidSnapRatiosAreFilteredByMinimumSizes()
    {
        PaneResizeTransaction transaction;
        VERIFY_IS_TRUE(transaction.Begin(1, 0.5, { 1000.0, 300.0, 100.0 }));
        const auto& ratios = transaction.ValidSnapRatios();
        VERIFY_IS_TRUE(std::find(ratios.begin(), ratios.end(), 0.25) == ratios.end());
        const auto update = transaction.Update(252.0, false);
        VERIFY_IS_FALSE(update.snapped);
        VERIFY_IS_TRUE(update.ratio >= 0.3);
    }

    void WinTermPaneResizeTests::AltBypassesSnapping()
    {
        PaneResizeTransaction transaction;
        VERIFY_IS_TRUE(transaction.Begin(1, 0.4, StandardGeometry));
        const auto update = transaction.Update(503.0, true);
        VERIFY_IS_FALSE(update.snapped);
        VERIFY_IS_TRUE(Near(0.503, update.ratio));
        VERIFY_IS_TRUE(update.indicator.empty());
    }

    void WinTermPaneResizeTests::HysteresisUsesLargerReleaseThreshold()
    {
        PaneResizeTransaction transaction;
        VERIFY_IS_TRUE(transaction.Begin(1, 0.4, StandardGeometry));
        VERIFY_IS_TRUE(transaction.Update(497.0, false).snapped);

        const auto held = transaction.Update(512.0, false);
        VERIFY_IS_TRUE(held.snapped);
        VERIFY_IS_TRUE(Near(0.5, held.ratio));

        const auto released = transaction.Update(516.0, false);
        VERIFY_IS_FALSE(released.snapped);
        VERIFY_IS_TRUE(Near(0.516, released.ratio));
    }

    void WinTermPaneResizeTests::CancelRestoresOriginalRatio()
    {
        PaneResizeTransaction transaction;
        VERIFY_IS_TRUE(transaction.Begin(77, 0.41, StandardGeometry));
        transaction.Update(750.0, false);
        const auto restored = transaction.Cancel();
        VERIFY_IS_TRUE(restored.has_value());
        VERIFY_IS_TRUE(Near(0.41, *restored));
        VERIFY_ARE_EQUAL(
            static_cast<int>(ResizeTransactionState::Cancelled),
            static_cast<int>(transaction.State()));
    }

    void WinTermPaneResizeTests::NestedResizeUsesOwningSplitGeometry()
    {
        PaneResizeTransaction nested;
        VERIFY_IS_TRUE(nested.Begin(2, 0.4, { 400.0, 80.0, 80.0 }));
        const auto update = nested.Update(202.0, false);
        VERIFY_IS_TRUE(update.snapped);
        VERIFY_IS_TRUE(Near(0.5, update.ratio));

        PaneResizeTransaction windowSized;
        VERIFY_IS_TRUE(windowSized.Begin(3, 0.4, StandardGeometry));
        const auto unrelated = windowSized.Update(202.0, false);
        VERIFY_IS_FALSE(unrelated.snapped);
        VERIFY_IS_TRUE(Near(0.202, unrelated.ratio));
    }

    void WinTermPaneResizeTests::LogicalThresholdIsStableAcrossDpiScales()
    {
        PaneResizeTransaction at100Percent;
        PaneResizeTransaction at200Percent;
        VERIFY_IS_TRUE(at100Percent.Begin(1, 0.4, StandardGeometry));
        VERIFY_IS_TRUE(at200Percent.Begin(2, 0.4, StandardGeometry));

        const auto physicalAt100 = 494.0;
        const auto physicalAt200 = 988.0;
        const auto logicalAt100 = physicalAt100 / 1.0;
        const auto logicalAt200 = physicalAt200 / 2.0;
        const auto first = at100Percent.Update(logicalAt100, false);
        const auto second = at200Percent.Update(logicalAt200, false);
        VERIFY_ARE_EQUAL(first.snapped, second.snapped);
        VERIFY_IS_TRUE(Near(first.ratio, second.ratio));
    }

    void WinTermPaneResizeTests::CustomRatioValidationRejectsUnsafeInput()
    {
        PaneResizeSettings settings;
        settings.preset = SnapPointPreset::Custom;
        settings.customRatios = {
            0.5,
            0.5,
            std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(),
            0.01,
        };
        VERIFY_IS_FALSE(settings.Validate().empty());

        settings.Normalize();
        VERIFY_ARE_EQUAL(size_t{ 1 }, settings.customRatios.size());
        VERIFY_IS_TRUE(Near(0.5, settings.customRatios.front()));
        VERIFY_IS_TRUE(settings.Validate().empty());
    }

    void WinTermPaneResizeTests::OneCommittedDragCreatesOneUndoEntry()
    {
        PaneResizeTransaction transaction;
        PaneResizeHistory history;
        VERIFY_IS_TRUE(transaction.Begin(9, 0.4, StandardGeometry));
        transaction.Update(430.0, false);
        transaction.Update(470.0, false);
        transaction.Update(503.0, false);
        const auto committed = transaction.Commit();
        VERIFY_IS_TRUE(committed.has_value());
        VERIFY_IS_TRUE(history.Record({ 9, transaction.OriginalRatio(), *committed }));
        VERIFY_ARE_EQUAL(size_t{ 1 }, history.UndoCount());

        const auto undo = history.Undo();
        VERIFY_IS_TRUE(undo.has_value());
        VERIFY_IS_TRUE(Near(0.4, undo->before));
        VERIFY_IS_TRUE(Near(0.5, undo->after));
        VERIFY_ARE_EQUAL(size_t{ 1 }, history.RedoCount());
    }

    void WinTermPaneResizeTests::NewResizeClearsRedo()
    {
        PaneResizeHistory history;
        VERIFY_IS_TRUE(history.Record({ 1, 0.4, 0.5 }));
        VERIFY_IS_TRUE(history.Undo().has_value());
        VERIFY_ARE_EQUAL(size_t{ 1 }, history.RedoCount());
        VERIFY_IS_TRUE(history.Record({ 2, 0.5, 0.66 }));
        VERIFY_ARE_EQUAL(size_t{ 0 }, history.RedoCount());
        VERIFY_ARE_EQUAL(size_t{ 1 }, history.UndoCount());
    }

    void WinTermPaneResizeTests::FiveHundredResizesKeepHistoryBounded()
    {
        PaneResizeHistory history{ 20 };
        for (uint64_t index = 1; index <= 500; ++index)
        {
            PaneResizeTransaction transaction;
            VERIFY_IS_TRUE(transaction.Begin(index, 0.4, StandardGeometry));
            const auto committed = transaction.Update(500.0 + static_cast<double>(index % 7), false);
            VERIFY_IS_TRUE(transaction.Commit().has_value());
            VERIFY_IS_TRUE(history.Record({ index, 0.4, committed.ratio }));
        }
        VERIFY_ARE_EQUAL(size_t{ 20 }, history.UndoCount());
        VERIFY_ARE_EQUAL(size_t{ 0 }, history.RedoCount());
    }
}
