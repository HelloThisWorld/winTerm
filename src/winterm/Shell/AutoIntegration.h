// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace winTerm::Shell
{
    // Automatic shell integration rewrites only a bare PowerShell profile
    // commandline so the packaged winTerm.Shell module is imported at startup.
    // The rules are deliberately narrow:
    //
    // * Only powershell.exe and pwsh.exe are recognized, by executable
    //   basename, with or without the extension.
    // * The only arguments tolerated on the original commandline are -NoLogo
    //   and -NoExit. Any other argument means the user has customized the
    //   invocation, and the commandline is left untouched. In particular a
    //   -Command, -File, or -EncodedCommand invocation is never rewritten.
    // * The module manifest path and the session id are refused when they
    //   contain a quote character, so the injected fragment cannot be broken
    //   out of. Execution policy is never altered.
    // * A commandline that already mentions the module is left untouched, so
    //   a restarted connection that reuses a rewritten commandline is not
    //   rewritten twice.
    namespace details
    {
        inline constexpr wchar_t AsciiLower(const wchar_t value) noexcept
        {
            return value >= L'A' && value <= L'Z' ? value + (L'a' - L'A') : value;
        }

        inline bool EqualsInsensitive(const std::wstring_view left, const std::wstring_view right) noexcept
        {
            if (left.size() != right.size())
            {
                return false;
            }
            for (size_t i = 0; i < left.size(); ++i)
            {
                if (AsciiLower(left[i]) != AsciiLower(right[i]))
                {
                    return false;
                }
            }
            return true;
        }

        inline bool ContainsInsensitive(const std::wstring_view value, const std::wstring_view needle) noexcept
        {
            if (needle.empty() || needle.size() > value.size())
            {
                return false;
            }
            for (size_t i = 0; i + needle.size() <= value.size(); ++i)
            {
                if (EqualsInsensitive(value.substr(i, needle.size()), needle))
                {
                    return true;
                }
            }
            return false;
        }

        // Splits off the next space-delimited token, honoring double quotes.
        // Returns an empty view when the input is exhausted.
        inline std::wstring_view NextToken(std::wstring_view& remaining) noexcept
        {
            size_t first{};
            while (first < remaining.size() && (remaining[first] == L' ' || remaining[first] == L'\t'))
            {
                ++first;
            }
            if (first == remaining.size())
            {
                remaining = {};
                return {};
            }

            auto last = first;
            auto quoted = false;
            while (last < remaining.size())
            {
                const auto ch = remaining[last];
                if (ch == L'"')
                {
                    quoted = !quoted;
                }
                else if (!quoted && (ch == L' ' || ch == L'\t'))
                {
                    break;
                }
                ++last;
            }

            const auto token = remaining.substr(first, last - first);
            remaining.remove_prefix(last);
            return token;
        }

        inline std::wstring_view StripQuotes(std::wstring_view token) noexcept
        {
            if (token.size() >= 2 && token.front() == L'"' && token.back() == L'"')
            {
                token.remove_prefix(1);
                token.remove_suffix(1);
            }
            return token;
        }

        inline std::wstring_view ExecutableBasename(std::wstring_view executable) noexcept
        {
            const auto separator = executable.find_last_of(L"\\/");
            if (separator != std::wstring_view::npos)
            {
                executable.remove_prefix(separator + 1);
            }
            return executable;
        }
    }

    inline bool IsBarePowerShellCommandline(const std::wstring_view commandline) noexcept
    {
        auto remaining = commandline;
        const auto executable = details::StripQuotes(details::NextToken(remaining));
        if (executable.empty())
        {
            return false;
        }

        const auto basename = details::ExecutableBasename(executable);
        static constexpr std::array<std::wstring_view, 4> supported{
            L"powershell.exe", L"pwsh.exe", L"powershell", L"pwsh"
        };
        auto recognized = false;
        for (const auto candidate : supported)
        {
            recognized = recognized || details::EqualsInsensitive(basename, candidate);
        }
        if (!recognized)
        {
            return false;
        }

        static constexpr std::array<std::wstring_view, 2> tolerated{
            L"-nologo", L"-noexit"
        };
        for (auto token = details::NextToken(remaining); !token.empty(); token = details::NextToken(remaining))
        {
            auto allowed = false;
            for (const auto candidate : tolerated)
            {
                allowed = allowed || details::EqualsInsensitive(token, candidate);
            }
            if (!allowed)
            {
                return false;
            }
        }
        return true;
    }

    inline std::optional<std::wstring> BuildAutoIntegratedPowerShellCommandline(const std::wstring_view commandline,
                                                                                const std::wstring_view moduleManifestPath,
                                                                                const std::wstring_view sessionId)
    {
        if (moduleManifestPath.empty() || sessionId.empty())
        {
            return std::nullopt;
        }
        if (moduleManifestPath.find_first_of(L"'\"") != std::wstring_view::npos ||
            sessionId.find_first_of(L"'\" \t") != std::wstring_view::npos)
        {
            return std::nullopt;
        }
        if (details::ContainsInsensitive(commandline, L"winterm.shell"))
        {
            return std::nullopt;
        }
        if (!IsBarePowerShellCommandline(commandline))
        {
            return std::nullopt;
        }

        std::wstring integrated{ commandline };
        while (!integrated.empty() && (integrated.back() == L' ' || integrated.back() == L'\t'))
        {
            integrated.pop_back();
        }
        integrated.append(L" -NoExit -Command \"&{ $env:WINTERM_SESSION_ID='");
        integrated.append(sessionId);
        integrated.append(L"'; $env:WINTERM_INTEGRATION_VERSION='1'; Import-Module -Name '");
        integrated.append(moduleManifestPath);
        integrated.append(L"' -ErrorAction SilentlyContinue }\"");
        return integrated;
    }
}
