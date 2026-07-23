// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PaneResizeModel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

using namespace winTerm::PaneResize;

namespace
{
    constexpr size_t MaximumCustomSnapRatios{ 12 };
    constexpr double RatioEpsilon{ 0.00001 };

    bool NearlyEqual(const double left, const double right) noexcept
    {
        return std::abs(left - right) <= RatioEpsilon;
    }
}

void PaneResizeSettings::Normalize(
    const double minimumSafeRatio,
    const double maximumSafeRatio) noexcept
{
    const auto safeMinimum = std::clamp(minimumSafeRatio, 0.01, 0.49);
    const auto safeMaximum = std::clamp(maximumSafeRatio, 0.51, 0.99);

    customRatios.erase(
        std::remove_if(
            customRatios.begin(),
            customRatios.end(),
            [&](const double ratio) {
                return !std::isfinite(ratio) || ratio < safeMinimum || ratio > safeMaximum;
            }),
        customRatios.end());
    std::sort(customRatios.begin(), customRatios.end());
    customRatios.erase(
        std::unique(customRatios.begin(), customRatios.end(), NearlyEqual),
        customRatios.end());
    if (customRatios.size() > MaximumCustomSnapRatios)
    {
        customRatios.resize(MaximumCustomSnapRatios);
    }

    if (!std::isfinite(snapThreshold) || snapThreshold <= 0)
    {
        snapThreshold = Design::InteractionTokens::SnapThreshold;
    }
    if (!std::isfinite(snapReleaseThreshold) || snapReleaseThreshold <= snapThreshold)
    {
        snapReleaseThreshold = std::max(
            Design::InteractionTokens::SnapReleaseThreshold,
            snapThreshold + 1.0);
    }
    if (!std::isfinite(keyboardStep) || keyboardStep <= 0 || keyboardStep > 0.25)
    {
        keyboardStep = 0.02;
    }
    if (!std::isfinite(acceleratedKeyboardStep) ||
        acceleratedKeyboardStep < keyboardStep ||
        acceleratedKeyboardStep > 0.25)
    {
        acceleratedKeyboardStep = 0.05;
    }
}

std::vector<std::string> PaneResizeSettings::Validate(
    const double minimumSafeRatio,
    const double maximumSafeRatio) const
{
    std::vector<std::string> errors;
    if (!std::isfinite(snapThreshold) || snapThreshold <= 0)
    {
        errors.emplace_back("Snap threshold must be a positive logical-pixel value.");
    }
    if (!std::isfinite(snapReleaseThreshold) || snapReleaseThreshold <= snapThreshold)
    {
        errors.emplace_back("Snap release threshold must be greater than the snap threshold.");
    }
    if (!std::isfinite(keyboardStep) || keyboardStep <= 0 || keyboardStep > 0.25)
    {
        errors.emplace_back("Keyboard resize step must be between 0 and 0.25.");
    }
    if (!std::isfinite(acceleratedKeyboardStep) ||
        acceleratedKeyboardStep < keyboardStep ||
        acceleratedKeyboardStep > 0.25)
    {
        errors.emplace_back("Accelerated keyboard resize step must be at least the normal step and no more than 0.25.");
    }
    if (customRatios.size() > MaximumCustomSnapRatios)
    {
        errors.emplace_back("No more than 12 custom snap ratios are allowed.");
    }

    auto sorted = customRatios;
    std::sort(sorted.begin(), sorted.end());
    for (size_t index = 0; index < sorted.size(); ++index)
    {
        const auto ratio = sorted[index];
        if (!std::isfinite(ratio) || ratio < minimumSafeRatio || ratio > maximumSafeRatio)
        {
            errors.emplace_back("Custom snap ratios must be finite and within the safe pane-size range.");
            break;
        }
        if (index > 0 && NearlyEqual(sorted[index - 1], ratio))
        {
            errors.emplace_back("Custom snap ratios must not contain duplicates.");
            break;
        }
    }
    if (preset == SnapPointPreset::Custom && customRatios.empty())
    {
        errors.emplace_back("The Custom preset requires at least one valid snap ratio.");
    }
    return errors;
}

std::vector<double> PaneResizeSettings::Ratios() const
{
    switch (preset)
    {
    case SnapPointPreset::Balanced:
        return { 0.5 };
    case SnapPointPreset::Custom:
        return customRatios;
    case SnapPointPreset::CommonRatios:
    default:
        return {
            Design::InteractionTokens::CommonSnapRatios.begin(),
            Design::InteractionTokens::CommonSnapRatios.end(),
        };
    }
}

bool ResizeGeometry::IsValid() const noexcept
{
    return std::isfinite(containerLength) &&
           std::isfinite(minimumFirst) &&
           std::isfinite(minimumSecond) &&
           containerLength > 0 &&
           minimumFirst >= 0 &&
           minimumSecond >= 0 &&
           minimumFirst + minimumSecond <= containerLength;
}

double ResizeGeometry::MinimumRatio() const noexcept
{
    return IsValid() ? minimumFirst / containerLength : 0.5;
}

double ResizeGeometry::MaximumRatio() const noexcept
{
    return IsValid() ? 1.0 - (minimumSecond / containerLength) : 0.5;
}

PaneResizeTransaction::PaneResizeTransaction(PaneResizeSettings settings) :
    _settings{ std::move(settings) }
{
    _settings.Normalize();
}

bool PaneResizeTransaction::Begin(
    const uint64_t ownerId,
    const double originalRatio,
    const ResizeGeometry geometry)
{
    if (_state == ResizeTransactionState::Active ||
        ownerId == 0 ||
        !std::isfinite(originalRatio) ||
        !geometry.IsValid())
    {
        return false;
    }

    _geometry = geometry;
    _ownerId = ownerId;
    _originalRatio = std::clamp(originalRatio, geometry.MinimumRatio(), geometry.MaximumRatio());
    _currentRatio = _originalRatio;
    _activeSnapRatio.reset();
    _validSnapRatios.clear();

    if (_settings.enableSnapping)
    {
        const auto minimum = geometry.MinimumRatio();
        const auto maximum = geometry.MaximumRatio();
        for (const auto ratio : _settings.Ratios())
        {
            if (std::isfinite(ratio) && ratio >= minimum && ratio <= maximum)
            {
                _validSnapRatios.emplace_back(ratio);
            }
        }
        std::sort(_validSnapRatios.begin(), _validSnapRatios.end());
        _validSnapRatios.erase(
            std::unique(_validSnapRatios.begin(), _validSnapRatios.end(), NearlyEqual),
            _validSnapRatios.end());
    }

    _state = ResizeTransactionState::Active;
    return true;
}

ResizeUpdate PaneResizeTransaction::Update(
    const double pointerPosition,
    const bool altPressed)
{
    if (_state != ResizeTransactionState::Active ||
        !std::isfinite(pointerPosition) ||
        !_geometry.IsValid())
    {
        return _freeUpdate(_currentRatio);
    }

    const auto rawRatio = std::clamp(
        pointerPosition / _geometry.containerLength,
        _geometry.MinimumRatio(),
        _geometry.MaximumRatio());
    if (!_settings.enableSnapping ||
        (_settings.altDisablesSnapping && altPressed) ||
        _validSnapRatios.empty())
    {
        _activeSnapRatio.reset();
        return _freeUpdate(rawRatio);
    }

    if (_activeSnapRatio)
    {
        const auto releaseDistance = _settings.snapReleaseThreshold / _geometry.containerLength;
        if (std::abs(rawRatio - *_activeSnapRatio) <= releaseDistance)
        {
            return _snappedUpdate(*_activeSnapRatio);
        }
        _activeSnapRatio.reset();
    }

    const auto enterDistance = _settings.snapThreshold / _geometry.containerLength;
    auto selected = _validSnapRatios.end();
    auto selectedDistance = std::numeric_limits<double>::max();
    for (auto candidate = _validSnapRatios.begin(); candidate != _validSnapRatios.end(); ++candidate)
    {
        const auto distance = std::abs(rawRatio - *candidate);
        const auto candidateIsBalanced = NearlyEqual(*candidate, 0.5);
        const auto selectedIsBalanced =
            selected != _validSnapRatios.end() && NearlyEqual(*selected, 0.5);
        if (distance < selectedDistance ||
            (NearlyEqual(distance, selectedDistance) && candidateIsBalanced && !selectedIsBalanced))
        {
            selected = candidate;
            selectedDistance = distance;
        }
    }
    if (selected != _validSnapRatios.end() && selectedDistance <= enterDistance)
    {
        _activeSnapRatio = *selected;
        return _snappedUpdate(*selected);
    }
    return _freeUpdate(rawRatio);
}

std::optional<double> PaneResizeTransaction::Commit()
{
    if (_state != ResizeTransactionState::Active)
    {
        return std::nullopt;
    }
    _state = ResizeTransactionState::Committed;
    _activeSnapRatio.reset();
    return _currentRatio;
}

std::optional<double> PaneResizeTransaction::Cancel()
{
    if (_state != ResizeTransactionState::Active)
    {
        return std::nullopt;
    }
    _currentRatio = _originalRatio;
    _activeSnapRatio.reset();
    _state = ResizeTransactionState::Cancelled;
    return _originalRatio;
}

ResizeTransactionState PaneResizeTransaction::State() const noexcept
{
    return _state;
}

uint64_t PaneResizeTransaction::OwnerId() const noexcept
{
    return _ownerId;
}

double PaneResizeTransaction::OriginalRatio() const noexcept
{
    return _originalRatio;
}

double PaneResizeTransaction::CurrentRatio() const noexcept
{
    return _currentRatio;
}

const std::vector<double>& PaneResizeTransaction::ValidSnapRatios() const noexcept
{
    return _validSnapRatios;
}

std::string PaneResizeTransaction::IndicatorForRatio(const double ratio)
{
    if (NearlyEqual(ratio, 0.25))
    {
        return "1 / 4";
    }
    if (NearlyEqual(ratio, 1.0 / 3.0))
    {
        return "1 / 3";
    }
    if (NearlyEqual(ratio, 0.5))
    {
        return "50 / 50";
    }
    if (NearlyEqual(ratio, 2.0 / 3.0))
    {
        return "2 / 3";
    }
    if (NearlyEqual(ratio, 0.75))
    {
        return "3 / 4";
    }
    return std::to_string(static_cast<int>(std::round(ratio * 100.0))) + "%";
}

ResizeUpdate PaneResizeTransaction::_freeUpdate(const double ratio)
{
    _currentRatio = ratio;
    return { ratio, false, std::nullopt, {} };
}

ResizeUpdate PaneResizeTransaction::_snappedUpdate(const double ratio)
{
    _currentRatio = ratio;
    return {
        ratio,
        true,
        ratio,
        _settings.showSnapRatioIndicator ? IndicatorForRatio(ratio) : std::string{},
    };
}

PaneResizeHistory::PaneResizeHistory(const size_t limit) :
    _limit{ std::clamp<size_t>(limit, 1, 100) }
{
}

bool PaneResizeHistory::Record(PaneResizeHistoryEntry entry)
{
    if (entry.ownerId == 0 ||
        !std::isfinite(entry.before) ||
        !std::isfinite(entry.after) ||
        NearlyEqual(entry.before, entry.after))
    {
        return false;
    }
    _undo.emplace_back(entry);
    _redo.clear();
    _trim();
    return true;
}

std::optional<PaneResizeHistoryEntry> PaneResizeHistory::Undo()
{
    if (_undo.empty())
    {
        return std::nullopt;
    }
    auto entry = _undo.back();
    _undo.pop_back();
    _redo.emplace_back(entry);
    return entry;
}

std::optional<PaneResizeHistoryEntry> PaneResizeHistory::Redo()
{
    if (_redo.empty())
    {
        return std::nullopt;
    }
    auto entry = _redo.back();
    _redo.pop_back();
    _undo.emplace_back(entry);
    _trim();
    return entry;
}

size_t PaneResizeHistory::UndoCount() const noexcept
{
    return _undo.size();
}

size_t PaneResizeHistory::RedoCount() const noexcept
{
    return _redo.size();
}

void PaneResizeHistory::Clear() noexcept
{
    _undo.clear();
    _redo.clear();
}

void PaneResizeHistory::_trim()
{
    while (_undo.size() > _limit)
    {
        _undo.pop_front();
    }
    while (_redo.size() > _limit)
    {
        _redo.pop_front();
    }
}
