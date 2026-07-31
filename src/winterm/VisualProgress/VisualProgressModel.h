// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>

namespace winTerm::VisualProgress
{
    enum class ProgressMode : uint8_t
    {
        Hidden,
        Determinate,
        Indeterminate,
    };

    enum class ProgressStatus : uint8_t
    {
        Running,
        Waiting,
        Success,
        Error,
        Cancelled,
    };

    enum class ProgressSource : uint8_t
    {
        None,
        Taskbar,
        ShellIntegration,
    };

    // Values intentionally mirror the semantic state exposed by TerminalCore.
    enum class ShellLifecycleState : uint8_t
    {
        None = 0,
        Prompt = 1,
        CommandStart = 2,
        CommandExecuted = 3,
        CommandFinished = 4,
    };

    struct ProgressSnapshot
    {
        ProgressMode mode{ ProgressMode::Hidden };
        ProgressStatus status{ ProgressStatus::Cancelled };
        uint8_t value{};
        bool visible{};
        ProgressSource source{ ProgressSource::None };
        uint64_t sequence{};

        bool SamePresentation(const ProgressSnapshot& other) const noexcept
        {
            return mode == other.mode &&
                   status == other.status &&
                   value == other.value &&
                   visible == other.visible &&
                   source == other.source;
        }
    };

    inline bool IsFeatureEnabled(const bool settingEnabled, const std::wstring_view emergencyOverride) noexcept
    {
        return settingEnabled && emergencyOverride != L"1";
    }

    class ProgressStateMachine final
    {
    public:
        std::optional<ProgressSnapshot> SetEnabled(const bool enabled) noexcept
        {
            std::scoped_lock lock{ _mutex };
            if (_closed || _enabled == enabled)
            {
                return std::nullopt;
            }

            _enabled = enabled;
            if (!enabled)
            {
                _explicitActive = false;
                _shellSnapshot.reset();
                return _emit(HiddenSnapshot(ProgressStatus::Cancelled));
            }
            return std::nullopt;
        }

        std::optional<ProgressSnapshot> ApplyTaskbar(const uint64_t state, const uint64_t value) noexcept
        {
            std::unique_lock lock{ _mutex, std::try_to_lock };
            if (!lock.owns_lock() || !_enabled || _closed)
            {
                return std::nullopt;
            }

            const auto clamped = static_cast<uint8_t>(std::min<uint64_t>(value, 100));
            switch (state)
            {
            case 0: // Clear
                _explicitActive = false;
                return _emit(_shellSnapshot.value_or(HiddenSnapshot()));
            case 1: // Set
                _explicitActive = true;
                _explicitSnapshot = { ProgressMode::Determinate, ProgressStatus::Running, clamped, true, ProgressSource::Taskbar, 0 };
                break;
            case 2: // Error
                _explicitActive = true;
                _explicitSnapshot = { ProgressMode::Determinate, ProgressStatus::Error, _meaningfulValue(clamped), true, ProgressSource::Taskbar, 0 };
                break;
            case 3: // Indeterminate
                _explicitActive = true;
                _explicitSnapshot = { ProgressMode::Indeterminate, ProgressStatus::Running, 0, true, ProgressSource::Taskbar, 0 };
                break;
            case 4: // Paused
                _explicitActive = true;
                _explicitSnapshot = { ProgressMode::Determinate, ProgressStatus::Waiting, _meaningfulValue(clamped), true, ProgressSource::Taskbar, 0 };
                break;
            default:
                return std::nullopt;
            }
            return _emit(_explicitSnapshot);
        }

        std::optional<ProgressSnapshot> ApplyShellLifecycle(const ShellLifecycleState state, const int64_t exitCode) noexcept
        {
            std::unique_lock lock{ _mutex, std::try_to_lock };
            if (!lock.owns_lock() || !_enabled || _closed)
            {
                return std::nullopt;
            }

            switch (state)
            {
            case ShellLifecycleState::Prompt:
                _shellSnapshot.reset();
                break;
            case ShellLifecycleState::CommandStart:
            case ShellLifecycleState::CommandExecuted:
                _shellSnapshot = ProgressSnapshot{ ProgressMode::Indeterminate, ProgressStatus::Running, 0, true, ProgressSource::ShellIntegration, 0 };
                break;
            case ShellLifecycleState::CommandFinished:
                _shellSnapshot = ProgressSnapshot{
                    ProgressMode::Determinate,
                    exitCode > 0 ? ProgressStatus::Error : ProgressStatus::Success,
                    100,
                    true,
                    ProgressSource::ShellIntegration,
                    0
                };
                break;
            case ShellLifecycleState::None:
            default:
                return std::nullopt;
            }

            return _explicitActive ? std::nullopt : _emit(_shellSnapshot.value_or(HiddenSnapshot()));
        }

        std::optional<ProgressSnapshot> Reset() noexcept
        {
            std::scoped_lock lock{ _mutex };
            _explicitActive = false;
            _shellSnapshot.reset();
            return _emit(HiddenSnapshot());
        }

        std::optional<ProgressSnapshot> Close() noexcept
        {
            std::scoped_lock lock{ _mutex };
            if (_closed)
            {
                return std::nullopt;
            }
            _closed = true;
            _enabled = false;
            _explicitActive = false;
            _shellSnapshot.reset();
            return _emit(HiddenSnapshot(ProgressStatus::Cancelled));
        }

        ProgressSnapshot Current() const noexcept
        {
            std::scoped_lock lock{ _mutex };
            return _current;
        }

    private:
        static ProgressSnapshot HiddenSnapshot(const ProgressStatus status = ProgressStatus::Cancelled) noexcept
        {
            return { ProgressMode::Hidden, status, 0, false, ProgressSource::None, 0 };
        }

        uint8_t _meaningfulValue(const uint8_t value) const noexcept
        {
            if (value == 0 && _explicitSnapshot.mode == ProgressMode::Determinate && _explicitSnapshot.value > 0)
            {
                return _explicitSnapshot.value;
            }
            return value;
        }

        std::optional<ProgressSnapshot> _emit(ProgressSnapshot snapshot) noexcept
        {
            if (_current.SamePresentation(snapshot))
            {
                return std::nullopt;
            }
            snapshot.sequence = ++_sequence;
            _current = snapshot;
            return snapshot;
        }

        mutable std::mutex _mutex;
        bool _enabled{};
        bool _closed{};
        bool _explicitActive{};
        uint64_t _sequence{};
        ProgressSnapshot _current{};
        ProgressSnapshot _explicitSnapshot{};
        std::optional<ProgressSnapshot> _shellSnapshot;
    };

    // A one-element mailbox bounds cross-thread UI work. Newer updates replace
    // older pending updates; terminal correctness always has priority over visuals.
    class ProgressUpdateMailbox final
    {
    public:
        bool Publish(const ProgressSnapshot& snapshot) noexcept
        {
            std::unique_lock lock{ _mutex, std::try_to_lock };
            if (!lock.owns_lock() || _closed)
            {
                return false;
            }
            _pending = snapshot;
            return true;
        }

        std::optional<ProgressSnapshot> TakeLatest() noexcept
        {
            std::scoped_lock lock{ _mutex };
            auto latest = _pending;
            _pending.reset();
            return latest;
        }

        bool HasPending() const noexcept
        {
            std::scoped_lock lock{ _mutex };
            return _pending.has_value();
        }

        void Close() noexcept
        {
            std::scoped_lock lock{ _mutex };
            _closed = true;
            _pending.reset();
        }

        void Reopen() noexcept
        {
            std::scoped_lock lock{ _mutex };
            _closed = false;
            _pending.reset();
        }

    private:
        mutable std::mutex _mutex;
        bool _closed{};
        std::optional<ProgressSnapshot> _pending;
    };
}
