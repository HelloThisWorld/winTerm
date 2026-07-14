// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ShellIntegrationProtocol.h"

#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <limits>

using namespace winTerm::Shell;

namespace
{
    bool ContainsUnsafeControlCharacter(const std::wstring_view value) noexcept
    {
        return std::any_of(value.begin(), value.end(), [](const wchar_t character) {
            return character == L'\0' || (character < 0x20 && character != L'\t');
        });
    }

    bool HasPrefix(const std::wstring_view value, const std::wstring_view prefix) noexcept
    {
        return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
    }

    std::optional<uint32_t> ParseExitCode(const std::wstring_view value) noexcept
    {
        if (value.empty() || !std::all_of(value.begin(), value.end(), [](const wchar_t character) { return std::iswdigit(character) != 0; }))
        {
            return std::nullopt;
        }

        const std::wstring copiedValue{ value };
        wchar_t* end = nullptr;
        const auto parsed = std::wcstoul(copiedValue.c_str(), &end, 10);
        if (end != copiedValue.c_str() + copiedValue.size() || parsed > std::numeric_limits<uint32_t>::max())
        {
            return std::nullopt;
        }
        return static_cast<uint32_t>(parsed);
    }
}

std::optional<ShellProtocolEvent> winTerm::Shell::ClassifyShellIntegrationPayload(const std::wstring_view payload)
{
    if (payload.empty() || payload.size() > MaximumShellProtocolPayloadLength || ContainsUnsafeControlCharacter(payload))
    {
        return std::nullopt;
    }

    constexpr std::wstring_view directoryPrefix{ L"9;9;" };
    if (HasPrefix(payload, directoryPrefix))
    {
        auto directory = payload.substr(directoryPrefix.size());
        if (directory.size() >= 2 && directory.front() == L'"' && directory.back() == L'"')
        {
            directory = directory.substr(1, directory.size() - 2);
        }
        if (directory.empty() || ContainsUnsafeControlCharacter(directory))
        {
            return std::nullopt;
        }
        return ShellProtocolEvent{ ShellEventType::CurrentDirectory, std::nullopt, std::wstring{ directory } };
    }

    constexpr std::wstring_view marksPrefix{ L"133;" };
    if (!HasPrefix(payload, marksPrefix) || payload.size() < marksPrefix.size() + 1)
    {
        return std::nullopt;
    }

    const auto mark = payload[marksPrefix.size()];
    if (payload.size() == marksPrefix.size() + 1)
    {
        switch (mark)
        {
        case L'A':
            return ShellProtocolEvent{ ShellEventType::PromptStart, std::nullopt, {} };
        case L'B':
            return ShellProtocolEvent{ ShellEventType::CommandStart, std::nullopt, {} };
        case L'C':
            return ShellProtocolEvent{ ShellEventType::CommandExecuted, std::nullopt, {} };
        case L'D':
            return ShellProtocolEvent{ ShellEventType::CommandFinished, std::nullopt, {} };
        default:
            return std::nullopt;
        }
    }

    if (mark != L'D' || payload[marksPrefix.size() + 1] != L';')
    {
        return std::nullopt;
    }

    const auto exitCode = ParseExitCode(payload.substr(marksPrefix.size() + 2));
    if (!exitCode)
    {
        return std::nullopt;
    }
    return ShellProtocolEvent{ ShellEventType::CommandFinished, exitCode, {} };
}
