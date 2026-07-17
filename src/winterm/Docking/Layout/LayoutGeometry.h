// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "LayoutTree.h"

#include <string>
#include <vector>

namespace winTerm::Docking
{
    struct LayoutRect
    {
        double x{};
        double y{};
        double width{};
        double height{};
    };

    struct LayoutGeometryEntry
    {
        std::string path;
        std::string nodeId;
        Workspaces::LayoutNodeType type{ Workspaces::LayoutNodeType::Pane };
        LayoutRect bounds;
    };

    struct LayoutGeometryResult
    {
        std::vector<LayoutGeometryEntry> entries;
        bool valid{ true };
        std::string invalidReason;
    };

    struct LayoutGeometrySettings
    {
        double minimumPaneWidth{ 160.0 };
        double minimumPaneHeight{ 100.0 };
        double dpiScale{ 1.0 };
    };

    class LayoutGeometry
    {
    public:
        static LayoutGeometryResult Calculate(
            const LayoutNodePtr& root,
            LayoutRect bounds,
            const LayoutGeometrySettings& settings = {});
    };
}
