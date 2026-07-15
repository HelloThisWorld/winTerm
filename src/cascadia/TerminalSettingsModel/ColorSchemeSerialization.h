// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <json/json.h>

namespace winrt::Microsoft::Terminal::Settings::Model
{
    ColorScheme DeserializeColorScheme(const Json::Value& json);
    Json::Value SerializeColorScheme(const ColorScheme& scheme);
}
