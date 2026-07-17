// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "LayoutTree.h"

#include <string>

namespace winTerm::Docking
{
    struct LayoutNormalizationResult
    {
        LayoutNodePtr root;
        size_t collapsedNodes{};
        bool changed{ false };
    };

    class LayoutNormalizer
    {
    public:
        static LayoutNormalizationResult Normalize(
            const LayoutNodePtr& root,
            std::string fallbackSlotId = {});
    };
}
