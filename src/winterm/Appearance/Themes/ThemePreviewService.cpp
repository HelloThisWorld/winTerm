// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ThemePreviewService.h"

using namespace winTerm::Appearance;

ThemePreviewService::ThemePreviewService(ThemeRegistry& registry) noexcept :
    _registry{ &registry }
{
}

ThemePreviewService::~ThemePreviewService()
{
    Cancel();
}

void ThemePreviewService::Begin(const ThemeDescriptor& theme)
{
    _registry->SetTemporaryPreview(theme);
    _active = true;
}

std::optional<ThemeDescriptor> ThemePreviewService::Commit()
{
    if (!_active)
    {
        return std::nullopt;
    }
    auto result = _registry->TemporaryPreview();
    _registry->SetTemporaryPreview(std::nullopt);
    _active = false;
    return result;
}

void ThemePreviewService::Cancel() noexcept
{
    if (_active)
    {
        _registry->SetTemporaryPreview(std::nullopt);
        _active = false;
    }
}

bool ThemePreviewService::IsActive() const noexcept
{
    return _active;
}
