// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ShellSessionMetadata.h"

using namespace winTerm::Shell;

bool CurrentDirectoryState::IsTrustedLocalPath() const noexcept
{
    return kind == CurrentDirectoryKind::LocalWindows || kind == CurrentDirectoryKind::Unc;
}

void ShellSessionRegistry::Upsert(ShellSessionMetadata metadata)
{
    if (metadata.sessionId.empty())
    {
        return;
    }
    _sessions.insert_or_assign(metadata.sessionId, std::move(metadata));
}

bool ShellSessionRegistry::Remove(const std::wstring_view sessionId)
{
    return _sessions.erase(std::wstring{ sessionId }) != 0;
}

const ShellSessionMetadata* ShellSessionRegistry::Find(const std::wstring_view sessionId) const
{
    const auto found = _sessions.find(std::wstring{ sessionId });
    return found == _sessions.end() ? nullptr : &found->second;
}
