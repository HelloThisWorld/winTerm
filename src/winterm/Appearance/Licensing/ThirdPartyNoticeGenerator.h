// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "AssetLicenseRegistry.h"

#include <string>

namespace winTerm::Appearance::Licensing
{
    class ThirdPartyNoticeGenerator final
    {
    public:
        static std::string Generate(const AssetLicenseRegistry& registry);
    };
}
