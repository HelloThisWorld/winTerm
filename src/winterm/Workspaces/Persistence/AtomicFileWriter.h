// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace winTerm::Workspaces
{
    struct AtomicWriteResult
    {
        bool succeeded{ false };
        bool backupUpdated{ false };
        std::string error;
    };

    class AtomicFileWriter final
    {
    public:
        using ContentValidator = std::function<bool(std::string_view)>;

        static AtomicWriteResult Write(
            const std::filesystem::path& path,
            std::string_view content,
            const std::optional<std::filesystem::path>& backupPath = std::nullopt,
            const ContentValidator& validateExisting = {});
    };
}
