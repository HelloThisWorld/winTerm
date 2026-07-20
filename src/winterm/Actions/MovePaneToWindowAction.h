// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "MovePaneAction.h"

namespace winTerm::Actions
{
    class MovePaneToWindowAction
    {
    public:
        static MovePanePlan BuildPlan(const MovePaneContext& context)
        {
            return MovePaneAction::ToNewWindow(context);
        }
    };
}
