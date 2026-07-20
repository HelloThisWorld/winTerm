// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <string_view>

namespace winTerm::Actions
{
    struct OpenPaneMenuAction
    {
        static constexpr std::string_view CommandId{ "openPaneMenu" };
        static constexpr std::string_view AccessibleName{ "Open pane menu" };
    };
}
