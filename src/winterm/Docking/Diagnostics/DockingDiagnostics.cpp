// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "DockingDiagnostics.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <unordered_set>

using namespace winTerm::Docking;
using namespace winTerm::Workspaces;

namespace
{
    std::string_view StatusText(const DockingStatus status) noexcept
    {
        switch (status)
        {
        case DockingStatus::Ready: return "ready";
        case DockingStatus::Committed: return "committed";
        case DockingStatus::RolledBack: return "rolledBack";
        case DockingStatus::Cancelled: return "cancelled";
        case DockingStatus::Failed: return "failed";
        default: return "invalid";
        }
    }

    void InspectNode(
        const std::shared_ptr<LayoutNodeDescriptor>& node,
        const std::unordered_set<std::string>& declaredPaneIds,
        std::unordered_set<std::string>& referencedPaneIds,
        std::unordered_set<std::string>& slotIds,
        std::unordered_set<const LayoutNodeDescriptor*>& ancestors,
        const size_t depth,
        LayoutHealthReport& report)
    {
        report.maximumDepth = std::max(report.maximumDepth, depth);
        if (!node)
        {
            ++report.orphanLayoutNodeCount;
            report.errors.emplace_back("A layout child is missing.");
            return;
        }
        if (!ancestors.emplace(node.get()).second)
        {
            ++report.cycleCount;
            report.errors.emplace_back("A layout cycle was detected.");
            return;
        }
        ++report.nodeCount;
        switch (node->type)
        {
        case LayoutNodeType::Pane:
            ++report.paneCount;
            if (!referencedPaneIds.emplace(node->paneId).second)
            {
                ++report.duplicatePaneIdCount;
                report.errors.emplace_back("A pane ID is referenced more than once.");
            }
            if (!declaredPaneIds.contains(node->paneId))
            {
                ++report.orphanLayoutNodeCount;
                report.errors.emplace_back("A layout pane has no declared session.");
            }
            break;
        case LayoutNodeType::EmptySlot:
            ++report.emptySlotCount;
            if (!slotIds.emplace(node->slotId).second)
            {
                ++report.duplicateSlotIdCount;
                report.errors.emplace_back("An empty-slot ID is referenced more than once.");
            }
            break;
        case LayoutNodeType::Split:
            if (!std::isfinite(node->ratio) || node->ratio < MinimumSplitRatio || node->ratio > MaximumSplitRatio)
            {
                ++report.invalidRatioCount;
                report.errors.emplace_back("A split ratio is invalid.");
            }
            InspectNode(node->first, declaredPaneIds, referencedPaneIds, slotIds, ancestors, depth + 1, report);
            InspectNode(node->second, declaredPaneIds, referencedPaneIds, slotIds, ancestors, depth + 1, report);
            break;
        default:
            ++report.orphanLayoutNodeCount;
            report.errors.emplace_back("An unsupported layout node was detected.");
            break;
        }
        ancestors.erase(node.get());
    }
}

bool LayoutHealthReport::IsValid() const noexcept
{
    return errors.empty();
}

DockingDiagnostics::DockingDiagnostics(const size_t capacity) :
    _capacity{ std::max<size_t>(1, capacity) }
{
}

void DockingDiagnostics::Record(DockingTraceRecord record)
{
    record.errorCategory = RedactCategory(record.errorCategory);
    std::scoped_lock lock{ _mutex };
    while (_records.size() >= _capacity)
    {
        _records.pop_front();
    }
    _records.emplace_back(std::move(record));
}

std::vector<DockingTraceRecord> DockingDiagnostics::Snapshot() const
{
    std::scoped_lock lock{ _mutex };
    return { _records.begin(), _records.end() };
}

void DockingDiagnostics::Clear() noexcept
{
    std::scoped_lock lock{ _mutex };
    _records.clear();
}

std::string DockingDiagnostics::CopySafeText() const
{
    const auto records = Snapshot();
    std::ostringstream output;
    output << "winTerm Docking Diagnostics\n";
    output << "Records: " << records.size() << "\n";
    for (const auto& record : records)
    {
        output << ToString(record.sourceType) << " -> "
               << ToString(record.targetType) << " / "
               << ToString(record.zone) << " / "
               << StatusText(record.status) << " / "
               << record.durationMilliseconds << " ms / "
               << record.layoutNodeCount << " nodes";
        if (!record.errorCategory.empty())
        {
            output << " / " << record.errorCategory;
        }
        output << "\n";
    }
    return output.str();
}

LayoutHealthReport DockingDiagnostics::InspectLayout(const WorkspaceDescriptor& workspace)
{
    LayoutHealthReport report;
    std::unordered_set<std::string> globalPaneIds;
    std::unordered_set<std::string> globalSlotIds;
    for (const auto& window : workspace.windows)
    {
        for (const auto& tab : window.tabs)
        {
            std::unordered_set<std::string> declaredPaneIds;
            for (const auto& pane : tab.panes)
            {
                if (!globalPaneIds.emplace(pane.id).second)
                {
                    ++report.duplicatePaneIdCount;
                    report.errors.emplace_back("A pane ID is declared more than once.");
                }
                declaredPaneIds.emplace(pane.id);
            }

            std::unordered_set<std::string> referencedPaneIds;
            std::unordered_set<const LayoutNodeDescriptor*> ancestors;
            InspectNode(
                tab.layout,
                declaredPaneIds,
                referencedPaneIds,
                globalSlotIds,
                ancestors,
                1,
                report);
            for (const auto& paneId : declaredPaneIds)
            {
                if (!referencedPaneIds.contains(paneId))
                {
                    ++report.orphanSessionCount;
                    report.errors.emplace_back("A declared pane session is not present in its layout.");
                }
            }
        }
    }
    return report;
}

std::string DockingDiagnostics::RedactCategory(const std::string_view category)
{
    static constexpr std::array AllowedCategories{
        std::string_view{ "validation" },
        std::string_view{ "targetClosing" },
        std::string_view{ "sourceClosing" },
        std::string_view{ "sessionAttach" },
        std::string_view{ "windowCreation" },
        std::string_view{ "visualCommit" },
        std::string_view{ "ownership" },
        std::string_view{ "workspaceSave" },
        std::string_view{ "token" },
        std::string_view{ "unsupported" },
        std::string_view{ "none" },
    };
    for (const auto allowed : AllowedCategories)
    {
        if (category == allowed)
        {
            return std::string{ allowed };
        }
    }
    return category.empty() ? std::string{} : std::string{ "other" };
}
