// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Model/DockingModel.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace winTerm::Docking
{
    enum class DockDragState
    {
        Idle,
        PointerPressed,
        DragPending,
        Dragging,
        TargetAcquired,
        DropPending,
        Committing,
        Completed,
        Cancelled,
        Failed,
    };

    enum class DragCapability
    {
        Tab,
        Pane,
        LayoutSubtree,
    };

    enum class DragCancellationReason
    {
        None,
        Escape,
        SourceClosed,
        SourceWindowClosed,
        TargetClosed,
        TargetPaneClosed,
        DisplayChanged,
        ApplicationShutdown,
        SessionExited,
        PointerCaptureLost,
        OperatingSystemCancelled,
        InvalidDrop,
        TokenExpired,
        CommitFailed,
    };

    struct DragPoint
    {
        double x{};
        double y{};
    };

    struct DragThresholdSettings
    {
        double horizontalPixels{ 4.0 };
        double verticalPixels{ 4.0 };
        double dpiScale{ 1.0 };
        bool touch{ false };
        std::chrono::milliseconds touchLongPress{ 350 };
    };

    class DragThreshold
    {
    public:
        static bool Exceeded(
            DragPoint pressed,
            DragPoint current,
            std::chrono::milliseconds held,
            const DragThresholdSettings& settings) noexcept;
    };

    struct DragPayloadRecord
    {
        std::string sourceInstanceId;
        DockSource source;
        DragCapability capability{ DragCapability::Pane };
        std::chrono::steady_clock::time_point expiresAt;
    };

    class DragPayloadRegistry
    {
    public:
        explicit DragPayloadRegistry(std::chrono::milliseconds lifetime = std::chrono::seconds{ 30 });

        std::string Register(
            std::string sourceInstanceId,
            DockSource source,
            DragCapability capability,
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
        std::optional<DragPayloadRecord> Resolve(
            std::string_view token,
            std::string_view sourceInstanceId,
            DragCapability capability,
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const;
        std::optional<DragPayloadRecord> Consume(
            std::string_view token,
            std::string_view sourceInstanceId,
            DragCapability capability,
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
        bool Invalidate(std::string_view token) noexcept;
        size_t PurgeExpired(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) noexcept;
        size_t Size() const noexcept;

    private:
        static std::string _newToken();

        std::chrono::milliseconds _lifetime;
        mutable std::mutex _mutex;
        std::unordered_map<std::string, DragPayloadRecord> _records;
    };

    class DockDragStateMachine
    {
    public:
        DockDragState State() const noexcept;
        DragCancellationReason CancellationReason() const noexcept;
        std::string_view FailureReason() const noexcept;

        bool Transition(DockDragState next);
        bool Cancel(DragCancellationReason reason);
        bool Fail(std::string reason);
        bool Reset();

        static bool CanTransition(DockDragState current, DockDragState next) noexcept;

    private:
        DockDragState _state{ DockDragState::Idle };
        DragCancellationReason _cancellationReason{ DragCancellationReason::None };
        std::string _failureReason;
    };
}
