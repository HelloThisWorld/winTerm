// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../../winterm/VisualProgress/VisualProgressSamplerState.h"

#include <SafeDispatcherTimer.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

namespace winTerm::VisualProgress
{
    // One coordinator is owned by each TerminalPage. It exists only to sample
    // bounded UI-dispatch latency while visible progress is active; renderer
    // policy remains in VisualProgressPerformanceGovernor.
    class VisualProgressWindowCoordinator final : public std::enable_shared_from_this<VisualProgressWindowCoordinator>
    {
    public:
        using SampleCallback = std::function<void(std::chrono::milliseconds)>;

        static constexpr std::chrono::milliseconds SamplerInterval{ 1000 };

        static std::shared_ptr<VisualProgressWindowCoordinator> TryCreate(
            const winrt::Windows::UI::Core::CoreDispatcher& dispatcher,
            SampleCallback callback) noexcept
        {
            if (!dispatcher || !callback)
            {
                return nullptr;
            }

            try
            {
                return std::shared_ptr<VisualProgressWindowCoordinator>{
                    new VisualProgressWindowCoordinator{ dispatcher, std::move(callback) }
                };
            }
            catch (...)
            {
                return nullptr;
            }
        }

        ~VisualProgressWindowCoordinator()
        {
            Close();
        }

        VisualProgressWindowCoordinator(const VisualProgressWindowCoordinator&) = delete;
        VisualProgressWindowCoordinator& operator=(const VisualProgressWindowCoordinator&) = delete;

        void SetEligible(const bool eligible) noexcept
        {
            if (_state.Closed())
            {
                return;
            }
            if (!eligible)
            {
                _stop();
                return;
            }
            if (!_state.Start())
            {
                return;
            }
            // Count the state transition before any throwable timer setup so
            // the catch path's _stop() always balances this increment.
            _activeSamplerCount.fetch_add(1, std::memory_order_relaxed);

            try
            {
                _timer.Interval(winrt::Windows::Foundation::TimeSpan{ SamplerInterval });
                const auto weak = weak_from_this();
                _timer.Tick([weak](auto&&, auto&&) {
                    if (const auto self = weak.lock())
                    {
                        self->_queueProbe();
                    }
                });
                _timer.Start();
            }
            catch (...)
            {
                _stop();
            }
        }

        void Close() noexcept
        {
            if (_state.Closed())
            {
                return;
            }

            const auto wasRunning = _state.Close();
            _timer.Destroy();
            if (wasRunning)
            {
                _activeSamplerCount.fetch_sub(1, std::memory_order_relaxed);
            }
            _callback = {};
            _dispatcher = nullptr;
        }

        bool Running() const noexcept
        {
            return _state.Running();
        }

        bool ProbePending() const noexcept
        {
            return _state.ProbePending();
        }

        uint64_t Generation() const noexcept
        {
            return _state.Generation();
        }

        static uint32_t ActiveSamplerCount() noexcept
        {
            return _activeSamplerCount.load(std::memory_order_relaxed);
        }

    private:
        VisualProgressWindowCoordinator(
            winrt::Windows::UI::Core::CoreDispatcher dispatcher,
            SampleCallback callback) :
            _dispatcher{ std::move(dispatcher) },
            _callback{ std::move(callback) }
        {
        }

        void _stop() noexcept
        {
            const auto wasRunning = _state.Stop();
            _timer.Destroy();
            if (wasRunning)
            {
                _activeSamplerCount.fetch_sub(1, std::memory_order_relaxed);
            }
        }

        void _queueProbe() noexcept
        {
            uint64_t generation{};
            if (!_dispatcher || !_state.TryBeginProbe(generation))
            {
                return;
            }

            const auto postedAt = std::chrono::steady_clock::now();
            const auto weak = weak_from_this();
            try
            {
                _dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Low, [weak, generation, postedAt]() {
                    if (const auto self = weak.lock())
                    {
                        if (!self->_state.TryCompleteProbe(generation))
                        {
                            return;
                        }

                        const auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - postedAt);
                        try
                        {
                            self->_callback(latency);
                        }
                        catch (...)
                        {
                            // Sampling is advisory and must never affect the UI.
                        }
                    }
                });
            }
            catch (...)
            {
                _stop();
            }
        }

        inline static std::atomic<uint32_t> _activeSamplerCount{};

        SafeDispatcherTimer _timer;
        winrt::Windows::UI::Core::CoreDispatcher _dispatcher{ nullptr };
        SampleCallback _callback;
        VisualProgressSamplerState _state;
    };
}
