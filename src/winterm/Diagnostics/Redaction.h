// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <string>
#include <string_view>

namespace winTerm::Diagnostics
{
    class Redaction final
    {
    public:
        [[nodiscard]] static std::string ForDiagnosticBundle(std::string_view value);
        [[nodiscard]] static bool IsSafeDiagnosticValue(std::string_view value) noexcept;
    };
}
