// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "../Model/DockingModel.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace winTerm::Docking
{
    struct DockingTraceRecord
    {
        DockSourceType sourceType{ DockSourceType::Pane };
        DockTargetType targetType{ DockTargetType::Pane };
        DockZone zone{ DockZone::Center };
        DockingStatus status{ DockingStatus::Invalid };
        uint64_t durationMilliseconds{};
        size_t layoutNodeCount{};
        std::string errorCategory;
    };

    struct LayoutHealthReport
    {
        size_t nodeCount{};
        size_t paneCount{};
        size_t emptySlotCount{};
        size_t duplicatePaneIdCount{};
        size_t duplicateSlotIdCount{};
        size_t orphanLayoutNodeCount{};
        size_t orphanSessionCount{};
        size_t invalidRatioCount{};
        size_t cycleCount{};
        size_t maximumDepth{};
        std::vector<std::string> errors;

        bool IsValid() const noexcept;
    };

    class DockingDiagnostics
    {
    public:
        explicit DockingDiagnostics(size_t capacity = 100);

        void Record(DockingTraceRecord record);
        std::vector<DockingTraceRecord> Snapshot() const;
        void Clear() noexcept;
        std::string CopySafeText() const;

        static LayoutHealthReport InspectLayout(const Workspaces::WorkspaceDescriptor& workspace);
        static std::string RedactCategory(std::string_view category);

    private:
        size_t _capacity;
        mutable std::mutex _mutex;
        std::deque<DockingTraceRecord> _records;
    };
}
