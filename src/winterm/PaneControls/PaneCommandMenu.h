// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "PaneCommandModel.h"

namespace winTerm::PaneControls
{
    enum class PaneMenuInvocation
    {
        IconRightClick,
        OverflowButton,
        Keyboard,
    };

    struct PaneCommandMenu
    {
        static std::vector<PaneCommandDescriptor> Build(
            const PaneCommandContext& context,
            PaneMenuInvocation)
        {
            // All invocation paths intentionally use one command model.
            return PaneCommandModel::Build(context);
        }
    };
}
