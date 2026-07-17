// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "Redaction.h"

#include <regex>

using namespace winTerm::Diagnostics;

namespace
{
    std::string Replace(const std::string& value, const std::regex& pattern, const std::string_view replacement)
    {
        return std::regex_replace(value, pattern, std::string{ replacement });
    }
}

std::string Redaction::ForDiagnosticBundle(const std::string_view value)
{
    auto result = std::string{ value };
    result = Replace(result, std::regex{ R"(([A-Za-z]:\\Users\\)[^\\/\r\n]+)" }, "$1<user>");
    result = Replace(result, std::regex{ R"(\\\\[^\\\r\n]+\\[^\\\r\n]+)" }, R"(\\<server>\<share>)");
    result = Replace(result, std::regex{ R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})" }, "<email>");
    result = Replace(result, std::regex{ R"(bearer\s+[A-Za-z0-9._~+/-]+)", std::regex_constants::icase }, "Bearer <redacted>");
    result = Replace(result, std::regex{ R"((api[_-]?key|token|password)\s*[=:]\s*[^\s,;]+)", std::regex_constants::icase }, "<secret>");
    result = Replace(result, std::regex{ R"((server|host|data source)\s*=\s*[^;\s]+)", std::regex_constants::icase }, "<connection-endpoint>");
    return result;
}

bool Redaction::IsSafeDiagnosticValue(const std::string_view value) noexcept
{
    return value.find("\\Users\\") == std::string_view::npos &&
           value.find("@") == std::string_view::npos &&
           value.find("Bearer ") == std::string_view::npos;
}
