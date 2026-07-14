// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "ThemeDescriptor.h"

#include <map>

namespace winTerm::Appearance
{
    class ThemeRegistry final
    {
    public:
        explicit ThemeRegistry(std::string fallbackThemeId = "winterm.midnight");

        void RegisterBuiltIn(ThemeDescriptor theme);
        void RegisterBundled(ThemeDescriptor theme);
        std::string RegisterImported(ThemeDescriptor theme);
        bool RemoveImported(std::string_view id);
        ThemeDescriptor Duplicate(std::string_view id);

        const ThemeDescriptor* GetThemeById(std::string_view id) const noexcept;
        std::vector<ThemeDescriptor> ListThemes() const;
        const ThemeDescriptor* Resolve(std::optional<std::string_view> profileThemeId,
                                       std::optional<std::string_view> globalThemeId) const noexcept;

        void SetTemporaryPreview(std::optional<ThemeDescriptor> theme);
        const std::optional<ThemeDescriptor>& TemporaryPreview() const noexcept;
        const std::string& FallbackThemeId() const noexcept;

    private:
        void _registerProtected(ThemeDescriptor theme, ThemeSourceType expectedSource);
        std::string _uniqueImportedId(std::string_view requestedId) const;

        std::map<std::string, ThemeDescriptor, std::less<>> _themes;
        std::optional<ThemeDescriptor> _temporaryPreview;
        std::string _fallbackThemeId;
    };
}
