// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace winTerm::Shell
{
    enum class ShellType
    {
        Unknown,
        PowerShell,
        WindowsPowerShell,
        CommandPrompt,
        Wsl,
        GitBash,
        Ssh,
    };

    enum class CurrentDirectoryKind
    {
        Unknown,
        LocalWindows,
        Unc,
        Wsl,
        Remote,
    };

    enum class CommandExecutionState
    {
        Unknown,
        AtPrompt,
        Running,
        Finished,
    };

    enum class IntegrationHealth
    {
        Disabled,
        Pending,
        Healthy,
        Degraded,
        Failed,
    };

    struct CurrentDirectoryState
    {
        CurrentDirectoryKind kind{ CurrentDirectoryKind::Unknown };
        std::wstring value;

        bool IsTrustedLocalPath() const noexcept;
    };

    struct ShellSessionMetadata
    {
        std::wstring sessionId;
        std::wstring profileId;
        ShellType shellType{ ShellType::Unknown };
        uint32_t integrationVersion{};
        uint32_t capabilities{};
        CurrentDirectoryState currentDirectory;
        CommandExecutionState commandState{ CommandExecutionState::Unknown };
        std::optional<uint32_t> lastExitCode;
        std::optional<std::chrono::milliseconds> lastCommandDuration;
        IntegrationHealth integrationHealth{ IntegrationHealth::Disabled };
    };

    class ShellSessionRegistry
    {
    public:
        void Upsert(ShellSessionMetadata metadata);
        bool Remove(std::wstring_view sessionId);
        const ShellSessionMetadata* Find(std::wstring_view sessionId) const;

    private:
        std::map<std::wstring, ShellSessionMetadata, std::less<>> _sessions;
    };
}
