// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "MovePaneAction.h"

namespace winTerm::Actions
{
    class MovePaneToTabAction
    {
    public:
        static MovePanePlan BuildPlan(const MovePaneContext& context)
        {
            return MovePaneAction::ToNewTab(context);
        }
    };
}
