// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "ThemeRegistry.h"

namespace winTerm::Appearance
{
    class ThemePreviewService final
    {
    public:
        explicit ThemePreviewService(ThemeRegistry& registry) noexcept;
        ~ThemePreviewService();

        void Begin(const ThemeDescriptor& theme);
        std::optional<ThemeDescriptor> Commit();
        void Cancel() noexcept;
        bool IsActive() const noexcept;

    private:
        ThemeRegistry* _registry;
        bool _active{ false };
    };
}
