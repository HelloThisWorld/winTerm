// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "VisualProgressModel.h"

#include <chrono>
#include <cstdint>
#include <optional>

namespace winTerm::VisualProgress
{
    using AccessibilityTimestamp = std::chrono::milliseconds;

    enum class AccessibilityAnnouncement : uint8_t
    {
        None,
        Started,
        Progress25,
        Progress50,
        Progress75,
        Progress100,
        Waiting,
        Success,
        Error,
        Cancelled,
    };

    // Structural state for a XAML progress peer. Indeterminate progress never
    // exposes a numeric value, and this model contains no user or terminal text.
    struct AccessibilitySemantics
    {
        ProgressMode mode{ ProgressMode::Hidden };
        ProgressStatus status{ ProgressStatus::Cancelled };
        uint8_t value{};
        bool visible{};
        bool hasNumericValue{};

        constexpr bool IsIndeterminate() const noexcept
        {
            return visible && mode == ProgressMode::Indeterminate;
        }
    };

    struct AccessibilityUpdate
    {
        AccessibilitySemantics semantics{};
        AccessibilityAnnouncement announcement{ AccessibilityAnnouncement::None };

        constexpr bool ShouldAnnounce() const noexcept
        {
            return announcement != AccessibilityAnnouncement::None;
        }
    };

    // A pane-local deterministic announcement policy. Callers provide a
    // monotonic timestamp, so tests never depend on wall-clock sleeps. Updates
    // received while a pane is inactive or hidden are consumed as baseline and
    // are never replayed when the pane later becomes eligible.
    class VisualProgressAccessibilityPolicy final
    {
    public:
        inline static constexpr std::chrono::milliseconds MinimumAnnouncementInterval{ 4000 };

        AccessibilityUpdate Apply(const ProgressSnapshot& snapshot,
                                  const bool paneVisible,
                                  const bool paneActive,
                                  const AccessibilityTimestamp timestamp) noexcept
        {
            AccessibilityUpdate update{ _semantics(snapshot, paneVisible) };
            const auto progressVisible = snapshot.visible && snapshot.mode != ProgressMode::Hidden;
            const auto paneEligible = paneVisible && paneActive;

            if (!progressVisible)
            {
                const auto meaningfulCancellation = snapshot.status == ProgressStatus::Cancelled &&
                                                    _hadVisibleActiveWork &&
                                                    !_terminalObserved;
                _terminalObserved = _terminalObserved || meaningfulCancellation;
                if (meaningfulCancellation && paneEligible)
                {
                    update.announcement = AccessibilityAnnouncement::Cancelled;
                }
                _resetLifecycle();
                return update;
            }

            if (_terminalObserved &&
                snapshot.status != ProgressStatus::Success &&
                snapshot.status != ProgressStatus::Error &&
                snapshot.status != ProgressStatus::Cancelled)
            {
                // A provider can begin new work before a separate hidden update
                // arrives. Treat that transition as a new lifecycle.
                _resetLifecycle();
            }

            const auto terminal = _terminalAnnouncement(snapshot.status);
            if (terminal != AccessibilityAnnouncement::None)
            {
                _consumeMilestones(snapshot);
                _pendingAnnouncement = AccessibilityAnnouncement::None;
                _deferredAnnouncement = AccessibilityAnnouncement::None;
                _previousStatus = snapshot.status;
                _hasPreviousStatus = true;
                if (!_terminalObserved)
                {
                    _terminalObserved = true;
                    if (paneEligible)
                    {
                        update.announcement = terminal;
                    }
                }
                return update;
            }

            if (snapshot.status == ProgressStatus::Cancelled)
            {
                const auto meaningfulCancellation = _hadVisibleActiveWork && !_terminalObserved;
                _terminalObserved = true;
                _pendingAnnouncement = AccessibilityAnnouncement::None;
                _deferredAnnouncement = AccessibilityAnnouncement::None;
                _previousStatus = snapshot.status;
                _hasPreviousStatus = true;
                if (meaningfulCancellation && paneEligible)
                {
                    update.announcement = AccessibilityAnnouncement::Cancelled;
                }
                return update;
            }

            const auto started = !_started;
            _started = true;
            if (started)
            {
                _lifecycleBeganEligible = paneEligible;
            }
            _hadVisibleActiveWork = _hadVisibleActiveWork ||
                                    (_lifecycleBeganEligible &&
                                     paneEligible &&
                                     (snapshot.status == ProgressStatus::Running || snapshot.status == ProgressStatus::Waiting));

            if (snapshot.status != ProgressStatus::Waiting)
            {
                // A deferred Waiting notification is meaningful only while the
                // lifecycle is still waiting. Never speak it after work resumes.
                if (_pendingAnnouncement == AccessibilityAnnouncement::Waiting)
                {
                    _pendingAnnouncement = AccessibilityAnnouncement::None;
                }
                if (_deferredAnnouncement == AccessibilityAnnouncement::Waiting)
                {
                    _deferredAnnouncement = AccessibilityAnnouncement::None;
                }
            }

            const auto waitingEntry = snapshot.status == ProgressStatus::Waiting &&
                                      (!_hasPreviousStatus || _previousStatus != ProgressStatus::Waiting);
            const auto milestone = _consumeMilestones(snapshot);
            _previousStatus = snapshot.status;
            _hasPreviousStatus = true;

            // One update produces at most one notification. Starting a new
            // lifecycle takes precedence; simultaneous milestones still advance
            // the baseline and will not be replayed later.
            const auto candidate = started      ? AccessibilityAnnouncement::Started :
                                   waitingEntry ? AccessibilityAnnouncement::Waiting :
                                                  milestone;
            if (!paneEligible)
            {
                _pendingAnnouncement = AccessibilityAnnouncement::None;
                _deferredAnnouncement = AccessibilityAnnouncement::None;
                return update;
            }

            if (candidate != AccessibilityAnnouncement::None)
            {
                _pendingAnnouncement = candidate;
            }
            if (started && waitingEntry)
            {
                // Starting and entering Waiting are distinct useful events.
                // Keep one bounded deferred notification rather than either
                // speaking twice at once or permanently losing Waiting.
                _deferredAnnouncement = AccessibilityAnnouncement::Waiting;
            }
            if (_pendingAnnouncement != AccessibilityAnnouncement::None &&
                _nonterminalIntervalElapsed(timestamp))
            {
                _lastNonterminalAnnouncement = timestamp;
                update.announcement = _pendingAnnouncement;
                _pendingAnnouncement = _deferredAnnouncement;
                _deferredAnnouncement = AccessibilityAnnouncement::None;
            }
            return update;
        }

        void Reset() noexcept
        {
            _started = false;
            _lifecycleBeganEligible = false;
            _hadVisibleActiveWork = false;
            _terminalObserved = false;
            _hasPreviousStatus = false;
            _previousStatus = ProgressStatus::Cancelled;
            _milestoneMask = 0;
            _pendingAnnouncement = AccessibilityAnnouncement::None;
            _deferredAnnouncement = AccessibilityAnnouncement::None;
            _lastNonterminalAnnouncement.reset();
        }

        constexpr bool HasPendingAnnouncement() const noexcept
        {
            return _pendingAnnouncement != AccessibilityAnnouncement::None ||
                   _deferredAnnouncement != AccessibilityAnnouncement::None;
        }

    private:
        static AccessibilitySemantics _semantics(const ProgressSnapshot& snapshot, const bool paneVisible) noexcept
        {
            const auto visible = paneVisible && snapshot.visible && snapshot.mode != ProgressMode::Hidden;
            const auto mode = visible ? snapshot.mode : ProgressMode::Hidden;
            const auto value = snapshot.value > 100 ? uint8_t{ 100 } : snapshot.value;
            return {
                mode,
                snapshot.status,
                value,
                visible,
                visible && mode == ProgressMode::Determinate,
            };
        }

        static constexpr AccessibilityAnnouncement _terminalAnnouncement(const ProgressStatus status) noexcept
        {
            switch (status)
            {
            case ProgressStatus::Success:
                return AccessibilityAnnouncement::Success;
            case ProgressStatus::Error:
                return AccessibilityAnnouncement::Error;
            default:
                return AccessibilityAnnouncement::None;
            }
        }

        AccessibilityAnnouncement _consumeMilestones(const ProgressSnapshot& snapshot) noexcept
        {
            if (snapshot.mode != ProgressMode::Determinate)
            {
                return AccessibilityAnnouncement::None;
            }

            AccessibilityAnnouncement highest{};
            _consumeMilestone(snapshot.value, 25, 0x1, AccessibilityAnnouncement::Progress25, highest);
            _consumeMilestone(snapshot.value, 50, 0x2, AccessibilityAnnouncement::Progress50, highest);
            _consumeMilestone(snapshot.value, 75, 0x4, AccessibilityAnnouncement::Progress75, highest);
            _consumeMilestone(snapshot.value, 100, 0x8, AccessibilityAnnouncement::Progress100, highest);
            return highest;
        }

        void _consumeMilestone(const uint8_t value,
                               const uint8_t threshold,
                               const uint8_t bit,
                               const AccessibilityAnnouncement announcement,
                               AccessibilityAnnouncement& highest) noexcept
        {
            if (value >= threshold && (_milestoneMask & bit) == 0)
            {
                _milestoneMask |= bit;
                highest = announcement;
            }
        }

        bool _nonterminalIntervalElapsed(const AccessibilityTimestamp timestamp) const noexcept
        {
            return !_lastNonterminalAnnouncement ||
                   (timestamp >= *_lastNonterminalAnnouncement &&
                    timestamp - *_lastNonterminalAnnouncement >= MinimumAnnouncementInterval);
        }

        void _resetLifecycle() noexcept
        {
            _started = false;
            _lifecycleBeganEligible = false;
            _hadVisibleActiveWork = false;
            _terminalObserved = false;
            _hasPreviousStatus = false;
            _previousStatus = ProgressStatus::Cancelled;
            _milestoneMask = 0;
            _pendingAnnouncement = AccessibilityAnnouncement::None;
            _deferredAnnouncement = AccessibilityAnnouncement::None;
            // Preserve the audible throttle across adjacent lifecycles.
        }

        bool _started{};
        bool _lifecycleBeganEligible{};
        bool _hadVisibleActiveWork{};
        bool _terminalObserved{};
        bool _hasPreviousStatus{};
        ProgressStatus _previousStatus{ ProgressStatus::Cancelled };
        uint8_t _milestoneMask{};
        AccessibilityAnnouncement _pendingAnnouncement{ AccessibilityAnnouncement::None };
        AccessibilityAnnouncement _deferredAnnouncement{ AccessibilityAnnouncement::None };
        std::optional<AccessibilityTimestamp> _lastNonterminalAnnouncement;
    };
}
