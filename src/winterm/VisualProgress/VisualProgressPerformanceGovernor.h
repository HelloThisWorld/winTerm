// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "VisualProgressRenderModel.h"

#include <chrono>
#include <cstdint>
#include <optional>

namespace winTerm::VisualProgress
{
    enum class PerformanceMode : uint8_t
    {
        Automatic,
        Full,
        Balanced,
        Minimal,
    };

    // Platform code owns signal collection and scheduling. This structure is
    // deliberately value-only so policy tests never require XAML, a window,
    // a dispatcher, or a wall clock.
    struct PerformanceGovernorInputs
    {
        PerformanceMode mode{ PerformanceMode::Automatic };
        bool featureEnabled{ true };
        bool emergencyDisabled{};
        bool osAnimationsEnabled{ true };
        bool applicationAnimationsEnabled{ true };
        bool highContrast{};
        bool windowVisible{ true };
        bool windowMinimized{};
        bool windowFocused{ true };
        bool paneVisible{ true };
        bool paneActive{ true };
        bool progressVisible{};
        bool progressActive{};
        uint16_t visibleActiveProgressCount{};
        bool softwareRendering{};
        bool remoteSession{};
        bool effectsFast{ true };
        bool energySaver{};
        RenderTier capabilityCeiling{ RenderTier::Full };
    };

    struct PerformanceRuntimeEnvironment
    {
        bool featureEnabled{ true };
        bool emergencyDisabled{};
        bool windowMinimized{};
        uint16_t visibleActiveProgressCount{};
        bool softwareRendering{};
        bool remoteSession{};
        bool effectsFast{ true };
        bool energySaver{};
    };

    struct PerformanceGovernorDecision
    {
        RenderTier tier{ RenderTier::Disabled };
        bool present{};
        bool continuousAnimation{};
        bool sparks{};
        bool shouldSample{};
        bool observationAccepted{};
        bool tierChanged{};
    };

    constexpr RenderTier MostRestrictiveRenderTier(const RenderTier first, const RenderTier second) noexcept
    {
        return static_cast<uint8_t>(first) >= static_cast<uint8_t>(second) ? first : second;
    }

    constexpr RenderTier NextLowerAdaptiveRenderTier(const RenderTier tier) noexcept
    {
        const auto nextTier = NextLowerRenderTier(tier);
        return nextTier == RenderTier::Disabled ? RenderTier::Solid : nextTier;
    }

    constexpr RenderTier NextHigherRenderTier(const RenderTier tier) noexcept
    {
        switch (tier)
        {
        case RenderTier::Disabled:
            return RenderTier::Solid;
        case RenderTier::Solid:
            return RenderTier::StaticGradient;
        case RenderTier::StaticGradient:
            return RenderTier::NoSparks;
        case RenderTier::NoSparks:
        case RenderTier::Full:
        default:
            return RenderTier::Full;
        }
    }

    using PerformanceTimestamp = std::chrono::milliseconds;

    class VisualProgressPerformanceGovernor final
    {
    public:
        static constexpr auto MinimumSampleInterval = std::chrono::milliseconds{ 1000 };
        static constexpr auto UnhealthyDispatchLatency = std::chrono::milliseconds{ 100 };
        static constexpr auto HealthyDispatchLatency = std::chrono::milliseconds{ 40 };
        static constexpr auto RecoveryCooldown = std::chrono::seconds{ 10 };
        static constexpr uint8_t UnhealthySamplesToDegrade = 3;
        static constexpr uint8_t HealthySamplesToRecover = 15;

        static constexpr RenderTier ModeTier(const PerformanceMode mode,
                                             const uint16_t visibleActiveProgressCount) noexcept
        {
            switch (mode)
            {
            case PerformanceMode::Balanced:
                return RenderTier::NoSparks;
            case PerformanceMode::Minimal:
                return RenderTier::StaticGradient;
            case PerformanceMode::Full:
                return RenderTier::Full;
            case PerformanceMode::Automatic:
            default:
                if (visibleActiveProgressCount >= 4)
                {
                    return RenderTier::StaticGradient;
                }
                if (visibleActiveProgressCount >= 2)
                {
                    return RenderTier::NoSparks;
                }
                return RenderTier::Full;
            }
        }

        static constexpr RenderTier PolicyTier(const PerformanceGovernorInputs& inputs) noexcept
        {
            auto tier = ModeTier(inputs.mode, inputs.visibleActiveProgressCount);
            tier = MostRestrictiveRenderTier(tier, inputs.capabilityCeiling);

            if (inputs.highContrast)
            {
                tier = MostRestrictiveRenderTier(tier, RenderTier::Solid);
            }
            else
            {
                if (!inputs.osAnimationsEnabled ||
                    !inputs.applicationAnimationsEnabled ||
                    inputs.softwareRendering ||
                    inputs.remoteSession ||
                    inputs.energySaver)
                {
                    tier = MostRestrictiveRenderTier(tier, RenderTier::StaticGradient);
                }
                if (!inputs.effectsFast)
                {
                    tier = MostRestrictiveRenderTier(tier, RenderTier::NoSparks);
                }
            }

            return tier;
        }

        PerformanceGovernorDecision Evaluate(const PerformanceGovernorInputs& inputs) noexcept
        {
            if (inputs.visibleActiveProgressCount == 0)
            {
                ResetIdle();
            }
            return _decision(inputs);
        }

        PerformanceGovernorDecision ObserveDispatchLatency(
            const PerformanceGovernorInputs& inputs,
            const std::chrono::milliseconds latency,
            const PerformanceTimestamp now) noexcept
        {
            auto decision = Evaluate(inputs);
            if (!decision.shouldSample)
            {
                return decision;
            }

            if (_lastSample &&
                (now < *_lastSample || now - *_lastSample < MinimumSampleInterval))
            {
                return decision;
            }

            _lastSample = now;
            decision.observationAccepted = true;
            const auto previousTier = _adaptiveTier;

            if (latency >= UnhealthyDispatchLatency)
            {
                _healthySamples = 0;
                if (_unhealthySamples < UnhealthySamplesToDegrade)
                {
                    ++_unhealthySamples;
                }
                if (_unhealthySamples >= UnhealthySamplesToDegrade)
                {
                    const auto nextTier = NextLowerAdaptiveRenderTier(_adaptiveTier);
                    if (nextTier != _adaptiveTier)
                    {
                        _adaptiveTier = nextTier;
                        _lastTierChange = now;
                    }
                    _unhealthySamples = 0;
                }
            }
            else if (latency <= HealthyDispatchLatency)
            {
                _unhealthySamples = 0;
                if (_healthySamples < HealthySamplesToRecover)
                {
                    ++_healthySamples;
                }

                const auto cooldownComplete = !_lastTierChange ||
                                              (now >= *_lastTierChange && now - *_lastTierChange >= RecoveryCooldown);
                if (_healthySamples >= HealthySamplesToRecover && cooldownComplete)
                {
                    _adaptiveTier = NextHigherRenderTier(_adaptiveTier);
                    _healthySamples = 0;
                    if (_adaptiveTier != previousTier)
                    {
                        _lastTierChange = now;
                    }
                }
            }
            else
            {
                // Samples in the neutral band break both consecutive streaks.
                _unhealthySamples = 0;
                _healthySamples = 0;
            }

            decision = _decision(inputs);
            decision.observationAccepted = true;
            decision.tierChanged = _adaptiveTier != previousTier;
            return decision;
        }

        PerformanceGovernorDecision ObserveHardFailure(
            const PerformanceGovernorInputs& inputs,
            const PerformanceTimestamp now) noexcept
        {
            const auto previousTier = _adaptiveTier;
            _adaptiveTier = NextLowerRenderTier(_adaptiveTier);
            _healthySamples = 0;
            _unhealthySamples = 0;
            _lastSample.reset();
            _lastTierChange = now;

            auto decision = _decision(inputs);
            decision.observationAccepted = true;
            decision.tierChanged = _adaptiveTier != previousTier;
            return decision;
        }

        void ResetIdle() noexcept
        {
            _adaptiveTier = RenderTier::Full;
            _healthySamples = 0;
            _unhealthySamples = 0;
            _lastSample.reset();
            _lastTierChange.reset();
        }

        constexpr RenderTier AdaptiveTier() const noexcept
        {
            return _adaptiveTier;
        }

        constexpr uint8_t ConsecutiveHealthySamples() const noexcept
        {
            return _healthySamples;
        }

        constexpr uint8_t ConsecutiveUnhealthySamples() const noexcept
        {
            return _unhealthySamples;
        }

    private:
        PerformanceGovernorDecision _decision(const PerformanceGovernorInputs& inputs) const noexcept
        {
            PerformanceGovernorDecision decision;
            const auto featureAvailable = inputs.featureEnabled && !inputs.emergencyDisabled;
            decision.tier = MostRestrictiveRenderTier(PolicyTier(inputs), _adaptiveTier);
            decision.present = featureAvailable &&
                               inputs.windowVisible &&
                               !inputs.windowMinimized &&
                               inputs.paneVisible &&
                               inputs.progressVisible &&
                               decision.tier != RenderTier::Disabled;
            if (!decision.present)
            {
                decision.tier = RenderTier::Disabled;
            }

            decision.continuousAnimation = decision.present &&
                                           inputs.windowFocused &&
                                           inputs.paneActive &&
                                           inputs.osAnimationsEnabled &&
                                           inputs.applicationAnimationsEnabled &&
                                           !inputs.highContrast &&
                                           decision.tier < RenderTier::StaticGradient;
            decision.sparks = decision.continuousAnimation && decision.tier == RenderTier::Full;
            decision.shouldSample = decision.present &&
                                    inputs.progressActive &&
                                    inputs.visibleActiveProgressCount != 0 &&
                                    inputs.windowFocused &&
                                    inputs.paneActive &&
                                    decision.tier != RenderTier::Disabled;
            return decision;
        }

        RenderTier _adaptiveTier{ RenderTier::Full };
        uint8_t _healthySamples{};
        uint8_t _unhealthySamples{};
        std::optional<PerformanceTimestamp> _lastSample;
        std::optional<PerformanceTimestamp> _lastTierChange;
    };
}
