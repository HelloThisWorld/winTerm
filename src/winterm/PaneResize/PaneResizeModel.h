// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Design/InteractionTokens.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace winTerm::PaneResize
{
    enum class SnapPointPreset
    {
        Balanced,
        CommonRatios,
        Custom,
    };

    struct PaneResizeSettings
    {
        bool enableSnapping{ true };
        SnapPointPreset preset{ SnapPointPreset::CommonRatios };
        std::vector<double> customRatios;
        double snapThreshold{ Design::InteractionTokens::SnapThreshold };
        double snapReleaseThreshold{ Design::InteractionTokens::SnapReleaseThreshold };
        bool showSnapRatioIndicator{ true };
        bool altDisablesSnapping{ true };
        double keyboardStep{ 0.02 };
        double acceleratedKeyboardStep{ 0.05 };

        void Normalize(double minimumSafeRatio = 0.05, double maximumSafeRatio = 0.95) noexcept;
        std::vector<std::string> Validate(
            double minimumSafeRatio = 0.05,
            double maximumSafeRatio = 0.95) const;
        std::vector<double> Ratios() const;
    };

    struct ResizeGeometry
    {
        double containerLength{};
        double minimumFirst{};
        double minimumSecond{};

        bool IsValid() const noexcept;
        double MinimumRatio() const noexcept;
        double MaximumRatio() const noexcept;
    };

    struct ResizeUpdate
    {
        double ratio{ 0.5 };
        bool snapped{ false };
        std::optional<double> snapRatio;
        std::string indicator;
    };

    enum class ResizeTransactionState
    {
        Idle,
        Active,
        Committed,
        Cancelled,
    };

    class PaneResizeTransaction
    {
    public:
        explicit PaneResizeTransaction(PaneResizeSettings settings = {});

        bool Begin(uint64_t ownerId, double originalRatio, ResizeGeometry geometry);
        ResizeUpdate Update(double pointerPosition, bool altPressed);
        std::optional<double> Commit();
        std::optional<double> Cancel();

        ResizeTransactionState State() const noexcept;
        uint64_t OwnerId() const noexcept;
        double OriginalRatio() const noexcept;
        double CurrentRatio() const noexcept;
        const std::vector<double>& ValidSnapRatios() const noexcept;

        static std::string IndicatorForRatio(double ratio);

    private:
        ResizeUpdate _freeUpdate(double ratio);
        ResizeUpdate _snappedUpdate(double ratio);

        PaneResizeSettings _settings;
        ResizeGeometry _geometry;
        ResizeTransactionState _state{ ResizeTransactionState::Idle };
        uint64_t _ownerId{};
        double _originalRatio{ 0.5 };
        double _currentRatio{ 0.5 };
        std::optional<double> _activeSnapRatio;
        std::vector<double> _validSnapRatios;
    };

    struct PaneResizeHistoryEntry
    {
        uint64_t ownerId{};
        double before{ 0.5 };
        double after{ 0.5 };
    };

    class PaneResizeHistory
    {
    public:
        explicit PaneResizeHistory(size_t limit = 20);

        bool Record(PaneResizeHistoryEntry entry);
        std::optional<PaneResizeHistoryEntry> Undo();
        std::optional<PaneResizeHistoryEntry> Redo();
        size_t UndoCount() const noexcept;
        size_t RedoCount() const noexcept;
        void Clear() noexcept;

    private:
        void _trim();

        size_t _limit;
        std::deque<PaneResizeHistoryEntry> _undo;
        std::deque<PaneResizeHistoryEntry> _redo;
    };
}
