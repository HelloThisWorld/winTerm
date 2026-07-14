// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace winTerm::Shell
{
    inline constexpr uint32_t ShellProtocolVersion{ 1 };
    inline constexpr size_t MaximumShellProtocolPayloadLength{ 8192 };

    enum class ShellEventType
    {
        CurrentDirectory,
        PromptStart,
        CommandStart,
        CommandExecuted,
        CommandFinished,
    };

    struct ShellProtocolEvent
    {
        ShellEventType type;
        std::optional<uint32_t> exitCode;
        std::wstring currentDirectory;
    };

    // This classifier is for diagnostics and tests only. Terminal data is parsed by
    // the inherited Microsoft Terminal dispatch implementation.
    std::optional<ShellProtocolEvent> ClassifyShellIntegrationPayload(std::wstring_view payload);
}
