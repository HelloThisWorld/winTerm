// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include "FontDescriptor.h"

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace winTerm::Appearance
{
    enum class PrivateFontCollectionIssueSeverity
    {
        Warning,
        Error,
    };

    struct PrivateFontCollectionIssue
    {
        PrivateFontCollectionIssueSeverity severity{ PrivateFontCollectionIssueSeverity::Error };
        std::string fontId;
        std::string message;
    };

    struct PrivateFontCollectionLoadResult
    {
        size_t loaded{};
        size_t ignored{};
        std::vector<PrivateFontCollectionIssue> issues;

        bool HasUsableFonts() const noexcept;
    };

    // This class validates packaged files and exposes them to the existing
    // DirectWrite custom-collection integration point. It never installs fonts
    // into the Windows Fonts directory or mutates the system font collection.
    class PrivateFontCollection final
    {
    public:
        explicit PrivateFontCollection(std::string fallbackFamily = "Cascadia Mono");

        PrivateFontCollectionLoadResult Load(std::span<const FontDescriptor> bundledFonts,
                                             const std::filesystem::path& repositoryRoot,
                                             bool enabled);
        void Clear() noexcept;

        bool IsEnabled() const noexcept;
        bool IsReady() const noexcept;
        const std::vector<FontDescriptor>& Fonts() const noexcept;
        const std::string& FallbackFamily() const noexcept;
        std::string ResolveFamily(std::string_view requestedFamily) const;

    private:
        std::vector<FontDescriptor> _fonts;
        std::string _fallbackFamily;
        bool _enabled{ false };
    };
}
