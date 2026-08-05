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
        Provider,
        ShellIntegration,
    };

    enum class ProgressProvider : uint8_t
    {
        None,
        DockerPull,
        DockerBuildKit,
        Pip,
        Git,
        Curl,
        Wget,
        Npm,
        Pnpm,
        Yarn,
        Nvm,
        Maven,
        Gradle,
        Generic,
    };

    enum class ProviderConfidence : uint8_t
    {
        None,
        Low,
        Medium,
        High,
    };

    static_assert(static_cast<uint8_t>(ProgressProvider::Generic) < 16);
    static_assert(static_cast<uint8_t>(ProgressMode::Indeterminate) < 4);
    static_assert(static_cast<uint8_t>(ProgressStatus::Cancelled) < 8);

    struct ProviderProgress
    {
        ProgressProvider provider{ ProgressProvider::None };
        ProgressMode mode{ ProgressMode::Hidden };
        ProgressStatus status{ ProgressStatus::Cancelled };
        uint8_t value{};
        ProviderConfidence confidence{ ProviderConfidence::None };
        bool visible{};
        bool transient{};
        bool suppressible{};
        uint16_t stage{};
        uint32_t sequence{};
    };

    // Provider state crosses the TerminalControl ABI as one atomic value. The
    // payload contains structural progress metadata only--never terminal text.
    inline constexpr uint64_t PackProviderProgress(const ProviderProgress& progress) noexcept
    {
        return (static_cast<uint64_t>(progress.mode) & 0x3u) |
               ((static_cast<uint64_t>(progress.status) & 0x7u) << 2u) |
               ((static_cast<uint64_t>(progress.value) & 0x7fu) << 5u) |
               ((static_cast<uint64_t>(progress.provider) & 0xfu) << 12u) |
               ((static_cast<uint64_t>(progress.confidence) & 0x3u) << 16u) |
               (static_cast<uint64_t>(progress.transient) << 18u) |
               (static_cast<uint64_t>(progress.suppressible) << 19u) |
               (static_cast<uint64_t>(progress.visible) << 20u) |
               ((static_cast<uint64_t>(progress.stage) & 0xfffu) << 21u) |
               ((static_cast<uint64_t>(progress.sequence) & 0x7fffffffu) << 33u);
    }

    inline constexpr ProviderProgress UnpackProviderProgress(const uint64_t packed) noexcept
    {
        return {
            static_cast<ProgressProvider>((packed >> 12u) & 0xfu),
            static_cast<ProgressMode>(packed & 0x3u),
            static_cast<ProgressStatus>((packed >> 2u) & 0x7u),
            static_cast<uint8_t>((packed >> 5u) & 0x7fu),
            static_cast<ProviderConfidence>((packed >> 16u) & 0x3u),
            ((packed >> 20u) & 0x1u) != 0,
            ((packed >> 18u) & 0x1u) != 0,
            ((packed >> 19u) & 0x1u) != 0,
            static_cast<uint16_t>((packed >> 21u) & 0xfffu),
            static_cast<uint32_t>((packed >> 33u) & 0x7fffffffu),
        };
    }

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
        ProgressProvider provider{ ProgressProvider::None };
        ProviderConfidence confidence{ ProviderConfidence::None };
        bool transient{};
        bool suppressible{};
        uint16_t stage{};
        // Identifies the OSC 133 command a Shell Integration launch fallback
        // belongs to. Zero for every other source and shell state. The
        // renderer captures it when the launch presentation starts and hands
        // it back through ExpireShellLaunch, which makes stale one-shot
        // completions inert.
        uint64_t launchGeneration{};

        bool SamePresentation(const ProgressSnapshot& other) const noexcept
        {
            return mode == other.mode &&
                   status == other.status &&
                   value == other.value &&
                   visible == other.visible &&
                   source == other.source &&
                   provider == other.provider &&
                   confidence == other.confidence &&
                   transient == other.transient &&
                   suppressible == other.suppressible &&
                   stage == other.stage &&
                   launchGeneration == other.launchGeneration;
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
                _explicitSnapshot = {};
                _providerSnapshot.reset();
                _shellSnapshot.reset();
                _resetShellLaunchScope();
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
                _explicitSnapshot = {};
                return _emit(_fallbackSnapshot());
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

        std::optional<ProgressSnapshot> ApplyProvider(const ProviderProgress& progress) noexcept
        {
            std::unique_lock lock{ _mutex, std::try_to_lock };
            if (!lock.owns_lock() || !_enabled || _closed)
            {
                return std::nullopt;
            }

            if (!progress.visible || progress.mode == ProgressMode::Hidden || progress.provider == ProgressProvider::None)
            {
                _providerSnapshot.reset();
                return _explicitActive ? std::nullopt : _emit(_fallbackSnapshot());
            }

            _providerSnapshot = ProgressSnapshot{
                progress.mode,
                progress.status,
                static_cast<uint8_t>(std::min<uint16_t>(progress.value, 100)),
                true,
                ProgressSource::Provider,
                0,
                progress.provider,
                progress.confidence,
                progress.transient,
                progress.suppressible,
                progress.stage,
            };
            return _explicitActive ? std::nullopt : _emit(*_providerSnapshot);
        }

        std::optional<ProgressSnapshot> ResetProvider() noexcept
        {
            std::unique_lock lock{ _mutex, std::try_to_lock };
            if (!lock.owns_lock() || !_enabled || _closed)
            {
                return std::nullopt;
            }

            _providerSnapshot.reset();
            return _explicitActive ? std::nullopt : _emit(_fallbackSnapshot());
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
                _providerSnapshot.reset();
                _shellSnapshot.reset();
                _beginShellLaunchScope();
                break;
            case ShellLifecycleState::CommandStart:
                // OSC 133;B: the user is composing input at an interactive
                // prompt. Nothing is executing, so an idle prompt must never
                // animate a bar; only CommandExecuted starts one.
                _shellSnapshot.reset();
                _beginShellLaunchScope();
                break;
            case ShellLifecycleState::CommandExecuted:
                // The launch fallback is a bounded one-shot per command. A
                // repeated CommandExecuted observation (pane rehydration,
                // alternate-screen churn, reconnect re-broadcast) belongs to
                // the same still-running command, so it must neither restart
                // a consumed launch nor open a new launch generation.
                if (_shellLifecycle == ShellLifecycleState::CommandExecuted)
                {
                    break;
                }
                _beginShellLaunchScope();
                _shellSnapshot = ProgressSnapshot{ ProgressMode::Indeterminate, ProgressStatus::Running, 0, true, ProgressSource::ShellIntegration, 0 };
                _shellSnapshot->launchGeneration = _shellLaunchGeneration;
                break;
            case ShellLifecycleState::CommandFinished:
                _providerSnapshot.reset();
                // Completion supersedes the launch immediately and advances
                // the launch scope, so a one-shot completion still in flight
                // can never disturb this terminal presentation.
                _beginShellLaunchScope();
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

            _shellLifecycle = state;
            return _explicitActive ? std::nullopt : _emit(_fallbackSnapshot());
        }

        // Called when the renderer's one-shot launch presentation has run for
        // one full traversal. The captured generation makes stale completions
        // inert: a callback from an earlier command can never hide progress
        // belonging to a newer command. Expiration clears the stored fallback
        // even while a provider or explicit source owns the presentation, so
        // a later ownership release cannot resurrect an expired launch.
        std::optional<ProgressSnapshot> ExpireShellLaunch(const uint64_t generation) noexcept
        {
            std::scoped_lock lock{ _mutex };
            if (!_enabled || _closed || generation == 0 ||
                generation != _shellLaunchGeneration || _shellLaunchExpired)
            {
                return std::nullopt;
            }
            if (!_shellSnapshot ||
                _shellSnapshot->source != ProgressSource::ShellIntegration ||
                _shellSnapshot->mode != ProgressMode::Indeterminate ||
                _shellSnapshot->status != ProgressStatus::Running)
            {
                // The launch fallback was already replaced; success, error,
                // and cancelled presentations are never expired.
                return std::nullopt;
            }

            _shellLaunchExpired = true;
            _shellSnapshot.reset();
            return _explicitActive ? std::nullopt : _emit(_fallbackSnapshot());
        }

        std::optional<ProgressSnapshot> Reset() noexcept
        {
            std::scoped_lock lock{ _mutex };
            _explicitActive = false;
            _explicitSnapshot = {};
            _providerSnapshot.reset();
            _shellSnapshot.reset();
            _resetShellLaunchScope();
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
            _explicitSnapshot = {};
            _providerSnapshot.reset();
            _shellSnapshot.reset();
            _resetShellLaunchScope();
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

        ProgressSnapshot _fallbackSnapshot() const noexcept
        {
            if (_providerSnapshot)
            {
                return *_providerSnapshot;
            }
            if (_shellSnapshot)
            {
                return *_shellSnapshot;
            }
            // An expired launch hides the bar while its command keeps
            // running. Running status keeps that hidden snapshot silent;
            // Cancelled would raise a fake interruption announcement through
            // the accessibility policy even though nothing was interrupted.
            if (_shellLifecycle == ShellLifecycleState::CommandExecuted && _shellLaunchExpired)
            {
                return HiddenSnapshot(ProgressStatus::Running);
            }
            return HiddenSnapshot();
        }

        // Every shell lifecycle transition opens a new launch scope: the
        // generation advance strands completion callbacks captured for an
        // earlier scope, and the cleared flag re-arms the one-shot for the
        // next CommandExecuted.
        void _beginShellLaunchScope() noexcept
        {
            ++_shellLaunchGeneration;
            _shellLaunchExpired = false;
        }

        void _resetShellLaunchScope() noexcept
        {
            _beginShellLaunchScope();
            _shellLifecycle = ShellLifecycleState::None;
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
        bool _shellLaunchExpired{};
        ShellLifecycleState _shellLifecycle{ ShellLifecycleState::None };
        uint64_t _sequence{};
        uint64_t _shellLaunchGeneration{};
        ProgressSnapshot _current{};
        ProgressSnapshot _explicitSnapshot{};
        std::optional<ProgressSnapshot> _providerSnapshot;
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
