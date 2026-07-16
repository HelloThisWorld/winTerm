// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "AtomicFileWriter.h"

#include <fstream>
#include <til/io.h>

using namespace winTerm::Workspaces;

namespace
{
    std::string ReadExisting(const std::filesystem::path& path)
    {
        std::error_code error;
        const auto size = std::filesystem::file_size(path, error);
        if (error)
        {
            return {};
        }
        std::ifstream input{ path, std::ios::binary };
        if (!input)
        {
            return {};
        }
        std::string content(static_cast<size_t>(size), '\0');
        if (!content.empty())
        {
            input.read(content.data(), static_cast<std::streamsize>(content.size()));
            if (input.gcount() != static_cast<std::streamsize>(content.size()))
            {
                return {};
            }
        }
        return content;
    }
}

AtomicWriteResult AtomicFileWriter::Write(
    const std::filesystem::path& path,
    const std::string_view content,
    const std::optional<std::filesystem::path>& backupPath,
    const ContentValidator& validateExisting)
{
    AtomicWriteResult result;
    try
    {
        if (path.empty())
        {
            result.error = "The atomic write target is empty.";
            return result;
        }
        if (const auto parent = path.parent_path(); !parent.empty())
        {
            std::filesystem::create_directories(parent);
        }

        if (backupPath && std::filesystem::is_regular_file(path))
        {
            const auto current = ReadExisting(path);
            const auto currentIsValid = !current.empty() && (!validateExisting || validateExisting(current));
            if (currentIsValid)
            {
                if (const auto parent = backupPath->parent_path(); !parent.empty())
                {
                    std::filesystem::create_directories(parent);
                }
                til::io::write_utf8_string_to_file_atomic(*backupPath, current);
                result.backupUpdated = true;
            }
        }

        til::io::write_utf8_string_to_file_atomic(path, content);
        result.succeeded = true;
    }
    catch (const std::exception& exception)
    {
        result.error = exception.what();
    }
    catch (...)
    {
        result.error = "The atomic workspace write failed.";
    }
    return result;
}
