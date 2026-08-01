// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <cstdint>

namespace winTerm::VisualProgress
{
    // Pure lifecycle state for the window-scoped dispatch-latency sampler.
    // Keeping timer and dispatcher ownership outside this type makes pending,
    // shutdown, and stale-generation behavior deterministic in unit tests.
    class VisualProgressSamplerState final
    {
    public:
        bool Start() noexcept
        {
            if (_closed || _running)
            {
                return false;
            }

            ++_generation;
            _probePending = false;
            _running = true;
            return true;
        }

        bool Stop() noexcept
        {
            ++_generation;
            _probePending = false;
            const auto wasRunning = _running;
            _running = false;
            return wasRunning;
        }

        bool Close() noexcept
        {
            if (_closed)
            {
                return false;
            }

            _closed = true;
            return Stop();
        }

        bool TryBeginProbe(uint64_t& generation) noexcept
        {
            if (_closed || !_running || _probePending)
            {
                return false;
            }

            _probePending = true;
            generation = _generation;
            return true;
        }

        bool TryCompleteProbe(const uint64_t generation) noexcept
        {
            if (_closed || !_running || generation != _generation)
            {
                return false;
            }

            _probePending = false;
            return true;
        }

        constexpr bool Running() const noexcept
        {
            return _running;
        }

        constexpr bool ProbePending() const noexcept
        {
            return _probePending;
        }

        constexpr bool Closed() const noexcept
        {
            return _closed;
        }

        constexpr uint64_t Generation() const noexcept
        {
            return _generation;
        }

    private:
        uint64_t _generation{};
        bool _probePending{};
        bool _running{};
        bool _closed{};
    };
}
