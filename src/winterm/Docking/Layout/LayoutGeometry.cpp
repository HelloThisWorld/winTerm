// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "LayoutGeometry.h"

#include <algorithm>
#include <cmath>

using namespace winTerm::Docking;
using namespace winTerm::Workspaces;

namespace
{
    void CalculateNode(
        const LayoutNodePtr& node,
        const LayoutRect bounds,
        const LayoutGeometrySettings& settings,
        const std::string& path,
        LayoutGeometryResult& result)
    {
        if (!node || !result.valid)
        {
            result.valid = false;
            result.invalidReason = "The proposed layout contains a missing node.";
            return;
        }
        if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) ||
            !std::isfinite(bounds.width) || !std::isfinite(bounds.height) ||
            bounds.width <= 0 || bounds.height <= 0)
        {
            result.valid = false;
            result.invalidReason = "The proposed layout has invalid geometry.";
            return;
        }

        const auto id = LayoutTree::NodeId(node);
        result.entries.emplace_back(LayoutGeometryEntry{
            path,
            id ? std::string{ *id } : std::string{},
            node->type,
            bounds,
        });
        if (node->type != LayoutNodeType::Split)
        {
            return;
        }

        const auto vertical = node->orientation == SplitOrientation::Vertical;
        const auto available = vertical ? bounds.width : bounds.height;
        const auto minimum = (vertical ? settings.minimumPaneWidth : settings.minimumPaneHeight) *
                             std::max(0.01, settings.dpiScale);
        if (available < minimum * 2.0)
        {
            result.valid = false;
            result.invalidReason = "The target is too small for the proposed split.";
            return;
        }
        const auto minimumRatio = minimum / available;
        const auto ratio = std::clamp(node->ratio, minimumRatio, 1.0 - minimumRatio);

        auto firstBounds = bounds;
        auto secondBounds = bounds;
        if (vertical)
        {
            firstBounds.width = bounds.width * ratio;
            secondBounds.x = bounds.x + firstBounds.width;
            secondBounds.width = bounds.width - firstBounds.width;
        }
        else
        {
            firstBounds.height = bounds.height * ratio;
            secondBounds.y = bounds.y + firstBounds.height;
            secondBounds.height = bounds.height - firstBounds.height;
        }
        CalculateNode(node->first, firstBounds, settings, path + ".first", result);
        CalculateNode(node->second, secondBounds, settings, path + ".second", result);
    }
}

LayoutGeometryResult LayoutGeometry::Calculate(
    const LayoutNodePtr& root,
    const LayoutRect bounds,
    const LayoutGeometrySettings& settings)
{
    LayoutGeometryResult result;
    if (!std::isfinite(settings.dpiScale) || settings.dpiScale <= 0 ||
        !std::isfinite(settings.minimumPaneWidth) || settings.minimumPaneWidth <= 0 ||
        !std::isfinite(settings.minimumPaneHeight) || settings.minimumPaneHeight <= 0)
    {
        result.valid = false;
        result.invalidReason = "The layout geometry settings are invalid.";
        return result;
    }
    CalculateNode(root, bounds, settings, "$", result);
    return result;
}
