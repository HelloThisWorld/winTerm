// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "RainbowArcVisualConstants.h"
#include "VisualProgressModel.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace winTerm::VisualProgress
{
    // Ordered from most capable to least capable. Degradation is local to the
    // affected renderer and never changes terminal behavior.
    enum class RenderTier : uint8_t
    {
        Full,
        NoSparks,
        StaticGradient,
        Solid,
        Disabled,
    };

    constexpr RenderTier NextLowerRenderTier(const RenderTier tier) noexcept
    {
        switch (tier)
        {
        case RenderTier::Full:
            return RenderTier::NoSparks;
        case RenderTier::NoSparks:
            return RenderTier::StaticGradient;
        case RenderTier::StaticGradient:
            return RenderTier::Solid;
        case RenderTier::Solid:
        case RenderTier::Disabled:
        default:
            return RenderTier::Disabled;
        }
    }

    struct RenderEnvironment
    {
        bool featureEnabled{ true };
        bool rendererAvailable{ true };
        bool hostLoaded{};
        bool tabVisible{};
        bool paneVisible{ true };
        bool paneActive{};
        bool windowVisible{ true };
        bool windowFocused{ true };
        bool animationsEnabled{ true };
        bool highContrast{};

        constexpr bool CanPresent() const noexcept
        {
            return featureEnabled && rendererAvailable && hostLoaded && tabVisible && paneVisible && windowVisible;
        }

        constexpr bool UsesStaticFallback() const noexcept
        {
            return highContrast || !animationsEnabled;
        }

        constexpr bool AllowsContinuousAnimation() const noexcept
        {
            return CanPresent() && paneActive && windowFocused && animationsEnabled && !highContrast;
        }
    };

    enum class RenderTransitionKind : uint8_t
    {
        None,
        Hide,
        ShowDeterminate,
        Advance,
        PhaseRegression,
        Indeterminate,
        Waiting,
        Success,
        Error,
        Cancelled,
        EnvironmentRefresh,
    };

    // Render timestamps are supplied by the caller. Tests can pass arbitrary
    // monotonically increasing values and never need wall-clock sleeps.
    using RenderTimestamp = std::chrono::milliseconds;

    struct RenderTransitionPlan
    {
        RenderTransitionKind kind{ RenderTransitionKind::None };
        RenderTier tier{ RenderTier::Disabled };
        ProgressMode mode{ ProgressMode::Hidden };
        ProgressStatus status{ ProgressStatus::Cancelled };
        float fromProgress{};
        float targetProgress{};
        std::chrono::milliseconds duration{};
        bool visible{};
        bool animateProgress{};
        bool phaseReset{};
        bool rainbowMoving{};
        bool indeterminateMoving{};
        bool headVisible{};
        bool breathe{};
        bool successSweep{};
        bool finalSparkBurst{};
        bool errorPulse{};
        bool errorWithoutProgress{};
        bool fadeOut{};
        bool sparksEligible{};
        bool staticFallback{};
        bool releaseAfterTransition{};
    };

    class VisualProgressRenderState final
    {
    public:
        explicit constexpr VisualProgressRenderState(const RenderTier tier = RenderTier::Full) noexcept :
            _tier{ tier }
        {
        }

        RenderTransitionPlan Apply(const ProgressSnapshot& snapshot,
                                   const RenderEnvironment& environment,
                                   const RenderTimestamp now) noexcept
        {
            if (_closed)
            {
                return _disabledPlan();
            }

            const auto current = CurrentProgress(now);
            const auto previous = _snapshot;
            _environment = environment;
            _snapshot = snapshot;

            auto plan = _basePlan(snapshot, current);
            const auto canShow = snapshot.visible && snapshot.mode != ProgressMode::Hidden && environment.CanPresent() && _tier != RenderTier::Disabled;
            plan.visible = canShow;

            if (!canShow)
            {
                plan.kind = previous.visible && snapshot.status == ProgressStatus::Cancelled ?
                                RenderTransitionKind::Cancelled :
                                RenderTransitionKind::Hide;
                plan.fadeOut = previous.visible &&
                               snapshot.status == ProgressStatus::Cancelled &&
                               environment.AllowsContinuousAnimation() &&
                               _tier < RenderTier::StaticGradient;
                plan.duration = plan.fadeOut ? RainbowArcVisualConstants::CancelFadeDuration : std::chrono::milliseconds::zero();
                plan.releaseAfterTransition = true;
                // A background tab or minimized window can receive its first
                // semantic update while it cannot present. Keep that latest
                // target so an environment refresh renders it immediately;
                // only an explicitly hidden snapshot clears progress.
                plan.targetProgress = _semanticTargetWhileHidden(snapshot, current);
                _setTransition(plan.targetProgress,
                               plan.targetProgress,
                               now,
                               std::chrono::milliseconds::zero());
                _presenting = false;
                return plan;
            }

            _presenting = true;
            plan.staticFallback = environment.UsesStaticFallback() || _tier >= RenderTier::StaticGradient;
            const auto motionAllowed = environment.AllowsContinuousAnimation() && _tier < RenderTier::StaticGradient;

            switch (snapshot.status)
            {
            case ProgressStatus::Waiting:
                plan.kind = RenderTransitionKind::Waiting;
                plan.targetProgress = _meaningfulProgress(snapshot, current);
                plan.headVisible = snapshot.mode == ProgressMode::Determinate && plan.targetProgress > 0.0f;
                plan.breathe = motionAllowed;
                plan.duration = std::chrono::milliseconds::zero();
                break;
            case ProgressStatus::Success:
                plan.kind = RenderTransitionKind::Success;
                plan.targetProgress = snapshot.mode == ProgressMode::Determinate ? 1.0f : current;
                plan.headVisible = plan.targetProgress > 0.0f;
                plan.animateProgress = motionAllowed && std::fabs(plan.targetProgress - current) > ProgressEpsilon;
                plan.duration = plan.animateProgress ? RainbowArcVisualConstants::SuccessAdvanceDuration : std::chrono::milliseconds::zero();
                plan.successSweep = motionAllowed;
                plan.finalSparkBurst = motionAllowed && environment.paneActive && _tier == RenderTier::Full;
                plan.fadeOut = motionAllowed;
                plan.releaseAfterTransition = true;
                break;
            case ProgressStatus::Error:
                plan.kind = RenderTransitionKind::Error;
                plan.targetProgress = _meaningfulProgress(snapshot, current);
                plan.errorWithoutProgress = plan.targetProgress <= ProgressEpsilon;
                // A zero value remains a real zero. The renderer may present a
                // status-only head/track treatment, but must not invent fill.
                plan.headVisible = snapshot.mode == ProgressMode::Determinate &&
                                   (plan.targetProgress > 0.0f || plan.errorWithoutProgress);
                plan.errorPulse = motionAllowed;
                plan.duration = std::chrono::milliseconds::zero();
                break;
            case ProgressStatus::Cancelled:
                plan.kind = RenderTransitionKind::Cancelled;
                plan.targetProgress = current;
                plan.fadeOut = motionAllowed;
                plan.duration = plan.fadeOut ? RainbowArcVisualConstants::CancelFadeDuration : std::chrono::milliseconds::zero();
                plan.releaseAfterTransition = true;
                break;
            case ProgressStatus::Running:
            default:
                if (snapshot.mode == ProgressMode::Indeterminate)
                {
                    plan.kind = RenderTransitionKind::Indeterminate;
                    plan.indeterminateMoving = motionAllowed;
                    plan.rainbowMoving = motionAllowed;
                    plan.headVisible = true;
                    plan.targetProgress = current;
                }
                else
                {
                    plan.targetProgress = _normalized(snapshot.value);
                    plan.headVisible = plan.targetProgress > 0.0f;
                    const auto regressed = _hadDeterminateValue && plan.targetProgress + ProgressEpsilon < _lastDeterminateTarget;
                    plan.kind = regressed ?
                                    RenderTransitionKind::PhaseRegression :
                                    (previous.visible ? RenderTransitionKind::Advance : RenderTransitionKind::ShowDeterminate);
                    plan.phaseReset = regressed;
                    plan.duration = regressed ?
                                        RainbowArcVisualConstants::RegressionInterpolationDuration :
                                        RainbowArcVisualConstants::DeterminateInterpolationDuration;
                    plan.animateProgress = motionAllowed && std::fabs(plan.targetProgress - current) > ProgressEpsilon;
                    if (!plan.animateProgress)
                    {
                        plan.duration = std::chrono::milliseconds::zero();
                    }
                    plan.rainbowMoving = motionAllowed;
                    _lastDeterminateTarget = plan.targetProgress;
                    _hadDeterminateValue = true;
                }
                break;
            }

            plan.sparksEligible = motionAllowed &&
                                  environment.paneActive &&
                                  snapshot.status == ProgressStatus::Running &&
                                  _tier == RenderTier::Full;

            _setTransition(current, plan.targetProgress, now, plan.duration);
            return plan;
        }

        RenderTransitionPlan RefreshEnvironment(const RenderEnvironment& environment,
                                                const RenderTimestamp now) noexcept
        {
            if (_closed)
            {
                return _disabledPlan();
            }

            const auto current = CurrentProgress(now);
            _environment = environment;
            auto plan = _basePlan(_snapshot, current);
            plan.kind = RenderTransitionKind::EnvironmentRefresh;
            plan.targetProgress = _targetProgress;
            plan.visible = _snapshot.visible && _snapshot.mode != ProgressMode::Hidden && environment.CanPresent() && _tier != RenderTier::Disabled;
            plan.staticFallback = environment.UsesStaticFallback() || _tier >= RenderTier::StaticGradient;

            const auto motionAllowed = environment.AllowsContinuousAnimation() && _tier < RenderTier::StaticGradient;
            plan.rainbowMoving = motionAllowed && _snapshot.status == ProgressStatus::Running;
            plan.indeterminateMoving = plan.rainbowMoving && _snapshot.mode == ProgressMode::Indeterminate;
            plan.breathe = motionAllowed && _snapshot.status == ProgressStatus::Waiting;
            plan.errorWithoutProgress = _snapshot.status == ProgressStatus::Error &&
                                        _targetProgress <= ProgressEpsilon;
            plan.sparksEligible = motionAllowed &&
                                  environment.paneActive &&
                                  _snapshot.status == ProgressStatus::Running &&
                                  _tier == RenderTier::Full;
            plan.headVisible = _snapshot.mode == ProgressMode::Indeterminate ||
                               _targetProgress > 0.0f ||
                               plan.errorWithoutProgress;
            if (!plan.visible)
            {
                plan.releaseAfterTransition = true;
            }

            // Environment changes pause or resume compositor work; they do not
            // replay semantic success/error/cancellation presentations.
            _setTransition(plan.staticFallback ? _targetProgress : current,
                           _targetProgress,
                           now,
                           std::chrono::milliseconds::zero());
            _presenting = plan.visible;
            return plan;
        }

        RenderTransitionPlan Degrade(const RenderTimestamp now) noexcept
        {
            _tier = NextLowerRenderTier(_tier);
            _environment.rendererAvailable = _tier != RenderTier::Disabled;
            return RefreshEnvironment(_environment, now);
        }

        void Tier(const RenderTier tier) noexcept
        {
            _tier = tier;
            _environment.rendererAvailable = tier != RenderTier::Disabled;
        }

        constexpr RenderTier Tier() const noexcept
        {
            return _tier;
        }

        float CurrentProgress(const RenderTimestamp now) const noexcept
        {
            if (_transitionDuration <= std::chrono::milliseconds::zero() || now <= _transitionStart)
            {
                return _transitionDuration <= std::chrono::milliseconds::zero() ? _targetProgress : _fromProgress;
            }

            const auto elapsed = now - _transitionStart;
            if (elapsed >= _transitionDuration)
            {
                return _targetProgress;
            }

            const auto ratio = static_cast<float>(elapsed.count()) / static_cast<float>(_transitionDuration.count());
            return std::clamp(_fromProgress + ((_targetProgress - _fromProgress) * ratio), 0.0f, 1.0f);
        }

        constexpr bool Presenting() const noexcept
        {
            return _presenting;
        }

        constexpr bool Closed() const noexcept
        {
            return _closed;
        }

        void Close() noexcept
        {
            _closed = true;
            _presenting = false;
            _snapshot = {};
            _fromProgress = 0.0f;
            _targetProgress = 0.0f;
            _transitionDuration = std::chrono::milliseconds::zero();
        }

    private:
        static constexpr float ProgressEpsilon{ 0.0001f };

        static constexpr float _normalized(const uint8_t value) noexcept
        {
            return static_cast<float>(std::min<uint8_t>(value, 100)) / 100.0f;
        }

        float _meaningfulProgress(const ProgressSnapshot& snapshot, const float current) const noexcept
        {
            if (snapshot.mode != ProgressMode::Determinate)
            {
                return current;
            }
            const auto normalized = _normalized(snapshot.value);
            if (normalized <= ProgressEpsilon && _hadDeterminateValue)
            {
                return _lastDeterminateTarget;
            }
            return normalized;
        }

        float _semanticTargetWhileHidden(const ProgressSnapshot& snapshot, const float current) noexcept
        {
            if (!snapshot.visible || snapshot.mode == ProgressMode::Hidden)
            {
                // Hidden is an ownership boundary, not merely a temporary
                // presentation pause. A later command must not inherit the
                // prior command's meaningful value or regression history.
                _hadDeterminateValue = false;
                _lastDeterminateTarget = 0.0f;
                return 0.0f;
            }
            if (snapshot.status == ProgressStatus::Cancelled || snapshot.mode == ProgressMode::Indeterminate)
            {
                return current;
            }
            if (snapshot.status == ProgressStatus::Success)
            {
                return snapshot.mode == ProgressMode::Determinate ? 1.0f : current;
            }

            const auto target = _meaningfulProgress(snapshot, current);
            if (snapshot.mode == ProgressMode::Determinate &&
                (snapshot.status == ProgressStatus::Running || target > ProgressEpsilon))
            {
                _lastDeterminateTarget = target;
                _hadDeterminateValue = true;
            }
            return target;
        }

        RenderTransitionPlan _basePlan(const ProgressSnapshot& snapshot, const float current) const noexcept
        {
            RenderTransitionPlan plan;
            plan.tier = _tier;
            plan.mode = snapshot.mode;
            plan.status = snapshot.status;
            plan.fromProgress = current;
            plan.targetProgress = current;
            return plan;
        }

        void _setTransition(const float from,
                            const float target,
                            const RenderTimestamp now,
                            const std::chrono::milliseconds duration) noexcept
        {
            _fromProgress = std::clamp(from, 0.0f, 1.0f);
            _targetProgress = std::clamp(target, 0.0f, 1.0f);
            _transitionStart = now;
            _transitionDuration = duration;
        }

        RenderTransitionPlan _disabledPlan() const noexcept
        {
            RenderTransitionPlan plan;
            plan.tier = RenderTier::Disabled;
            plan.releaseAfterTransition = true;
            return plan;
        }

        RenderTier _tier{ RenderTier::Full };
        RenderEnvironment _environment{};
        ProgressSnapshot _snapshot{};
        RenderTimestamp _transitionStart{};
        std::chrono::milliseconds _transitionDuration{};
        float _fromProgress{};
        float _targetProgress{};
        float _lastDeterminateTarget{};
        bool _hadDeterminateValue{};
        bool _presenting{};
        bool _closed{};
    };

    class SparkBudget final
    {
    public:
        explicit SparkBudget(const uint8_t capacity = static_cast<uint8_t>(RainbowArcVisualConstants::SparkCapacityPerWindowOrProcess)) noexcept :
            _capacity{ std::min<uint8_t>(capacity, static_cast<uint8_t>(RainbowArcVisualConstants::SparkCapacityPerWindowOrProcess)) }
        {
        }

        SparkBudget(const SparkBudget&) = delete;
        SparkBudget& operator=(const SparkBudget&) = delete;

        bool TryAcquire(const uint8_t count = 1) noexcept
        {
            if (count == 0)
            {
                return true;
            }

            auto current = _live.load(std::memory_order_relaxed);
            for (;;)
            {
                if (current > _capacity || count > static_cast<uint8_t>(_capacity - current))
                {
                    return false;
                }
                if (_live.compare_exchange_weak(current,
                                                static_cast<uint8_t>(current + count),
                                                std::memory_order_acq_rel,
                                                std::memory_order_relaxed))
                {
                    return true;
                }
            }
        }

        void Release(const uint8_t count = 1) noexcept
        {
            if (count == 0)
            {
                return;
            }

            auto current = _live.load(std::memory_order_relaxed);
            for (;;)
            {
                const auto next = current > count ? static_cast<uint8_t>(current - count) : uint8_t{};
                if (_live.compare_exchange_weak(current,
                                                next,
                                                std::memory_order_acq_rel,
                                                std::memory_order_relaxed))
                {
                    return;
                }
            }
        }

        uint8_t Live() const noexcept
        {
            return _live.load(std::memory_order_acquire);
        }

        constexpr uint8_t Capacity() const noexcept
        {
            return _capacity;
        }

    private:
        const uint8_t _capacity;
        std::atomic<uint8_t> _live{};
    };

    struct SparkHandle
    {
        uint8_t slot{ std::numeric_limits<uint8_t>::max() };
        uint32_t generation{};

        constexpr explicit operator bool() const noexcept
        {
            return slot < RainbowArcVisualConstants::SparkPoolCapacityPerPane;
        }
    };

    class SparkPool final
    {
    public:
        explicit SparkPool(SparkBudget& budget) noexcept :
            _budget{ &budget }
        {
        }

        ~SparkPool()
        {
            ReleaseAll();
        }

        SparkPool(const SparkPool&) = delete;
        SparkPool& operator=(const SparkPool&) = delete;

        std::optional<SparkHandle> Acquire(const RenderTimestamp now,
                                           std::chrono::milliseconds lifetime,
                                           const bool persistent = false) noexcept
        {
            if (!_budget || _live >= RainbowArcVisualConstants::SparkPoolCapacityPerPane)
            {
                return std::nullopt;
            }

            for (uint8_t index = 0; index < _slots.size(); ++index)
            {
                auto& slot = _slots[index];
                if (slot.active)
                {
                    continue;
                }
                if (!_budget->TryAcquire())
                {
                    return std::nullopt;
                }

                lifetime = std::clamp(lifetime,
                                      RainbowArcVisualConstants::MinimumSparkLifetime,
                                      RainbowArcVisualConstants::MaximumSparkLifetime);
                slot.active = true;
                slot.persistent = persistent;
                slot.expiresAt = persistent ? RenderTimestamp::max() : now + lifetime;
                ++slot.generation;
                ++_live;
                return SparkHandle{ index, slot.generation };
            }
            return std::nullopt;
        }

        bool Release(const SparkHandle handle) noexcept
        {
            if (!handle || handle.slot >= _slots.size())
            {
                return false;
            }

            auto& slot = _slots[handle.slot];
            if (!slot.active || slot.generation != handle.generation)
            {
                return false;
            }

            slot.active = false;
            slot.persistent = false;
            slot.expiresAt = {};
            --_live;
            if (_budget)
            {
                _budget->Release();
            }
            return true;
        }

        uint8_t ReleaseExpired(const RenderTimestamp now) noexcept
        {
            uint8_t released{};
            for (uint8_t index = 0; index < _slots.size(); ++index)
            {
                const auto& slot = _slots[index];
                if (slot.active && !slot.persistent && slot.expiresAt <= now)
                {
                    const auto generation = slot.generation;
                    released += Release({ index, generation }) ? 1 : 0;
                }
            }
            return released;
        }

        void ReleaseAll() noexcept
        {
            if (!_budget || _live == 0)
            {
                return;
            }

            const auto released = _live;
            for (auto& slot : _slots)
            {
                slot.active = false;
                slot.persistent = false;
                slot.expiresAt = {};
            }
            _live = 0;
            _budget->Release(released);
        }

        constexpr uint8_t Live() const noexcept
        {
            return _live;
        }

        constexpr bool Full() const noexcept
        {
            return _live >= RainbowArcVisualConstants::SparkPoolCapacityPerPane;
        }

        bool IsActive(const SparkHandle handle) const noexcept
        {
            return handle &&
                   handle.slot < _slots.size() &&
                   _slots[handle.slot].active &&
                   _slots[handle.slot].generation == handle.generation;
        }

    private:
        struct Slot
        {
            RenderTimestamp expiresAt{};
            uint32_t generation{};
            bool active{};
            bool persistent{};
        };

        SparkBudget* _budget{};
        std::array<Slot, RainbowArcVisualConstants::SparkPoolCapacityPerPane> _slots{};
        uint8_t _live{};
    };

    constexpr bool RequiresSparkWork(const RenderTransitionPlan& plan, const uint8_t liveSparkCount) noexcept
    {
        return plan.sparksEligible || plan.finalSparkBurst || liveSparkCount != 0;
    }
}
