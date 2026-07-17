// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Drag/DockDragSession.h"
#include "../Layout/LayoutGeometry.h"
#include "../Model/DockingModel.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace winTerm::Docking
{
    struct DockTargetCandidate
    {
        std::string key;
        DockTarget target;
        DockZone zone{ DockZone::Center };
        LayoutRect bounds;
        bool enabled{ true };
        std::string disabledReason;
    };

    struct DockTargetHysteresisSettings
    {
        std::chrono::milliseconds dwellTime{ 75 };
        double minimumDistance{ 8.0 };
    };

    class DockTargetResolver
    {
    public:
        explicit DockTargetResolver(DockTargetHysteresisSettings settings = {});

        std::optional<DockTargetCandidate> Resolve(
            const std::vector<DockTargetCandidate>& candidates,
            DragPoint pointer,
            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
        std::optional<DockTargetCandidate> Current() const;
        void Clear() noexcept;

        static int Priority(DockTargetType type) noexcept;

    private:
        static bool _contains(const LayoutRect& bounds, DragPoint pointer) noexcept;
        static double _distance(DragPoint first, DragPoint second) noexcept;

        DockTargetHysteresisSettings _settings;
        std::optional<DockTargetCandidate> _current;
        std::optional<DockTargetCandidate> _pending;
        DragPoint _pendingStart;
        std::chrono::steady_clock::time_point _pendingSince{};
    };
}
