// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "FontFallbackResolver.h"

#include <algorithm>
#include <cctype>

using namespace winTerm::Appearance;

namespace
{
    constexpr std::string_view CascadiaMonoNerdFont{ "Cascadia Mono NF" };
    constexpr std::string_view CascadiaMono{ "Cascadia Mono" };
    constexpr std::string_view SegoeSymbol{ "Segoe UI Symbol" };
    constexpr std::string_view SegoeEmoji{ "Segoe UI Emoji" };

    std::string FoldAscii(const std::string_view value)
    {
        std::string result{ value };
        std::transform(result.begin(), result.end(), result.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return result;
    }

    const std::string* FindAvailable(const std::span<const std::string> availableFamilies, const std::string_view requested)
    {
        const auto folded = FoldAscii(requested);
        const auto found = std::find_if(availableFamilies.begin(), availableFamilies.end(), [&](const auto& family) {
            return FoldAscii(family) == folded;
        });
        return found == availableFamilies.end() ? nullptr : &*found;
    }

    void AddUnique(std::vector<std::string>& chain, const std::string_view family)
    {
        const auto folded = FoldAscii(family);
        const auto duplicate = std::any_of(chain.begin(), chain.end(), [&](const auto& existing) {
            return FoldAscii(existing) == folded;
        });
        if (!duplicate)
        {
            chain.emplace_back(family);
        }
    }
}

std::vector<std::string> FontFallbackResolver::Resolve(const FontRegistry& registry,
                                                       const std::string_view selectedFamily,
                                                       const FontFallbackOptions options)
{
    std::vector<std::string> availableFamilies;
    for (const auto& descriptor : registry.ListFonts())
    {
        AddUnique(availableFamilies, descriptor.familyName);
    }
    return Resolve(availableFamilies, selectedFamily, options);
}

std::vector<std::string> FontFallbackResolver::Resolve(const std::span<const std::string> availableFamilies,
                                                       const std::string_view selectedFamily,
                                                       const FontFallbackOptions options)
{
    std::vector<std::string> chain;
    if (const auto selected = FindAvailable(availableFamilies, selectedFamily))
    {
        AddUnique(chain, *selected);
    }

    // Cascadia Mono NF is never advertised unless it was actually discovered.
    if (options.fallbackEnabled)
    {
        if (const auto nerdFont = FindAvailable(availableFamilies, CascadiaMonoNerdFont))
        {
            AddUnique(chain, *nerdFont);
        }
    }

    if (const auto cascadiaMono = FindAvailable(availableFamilies, CascadiaMono))
    {
        AddUnique(chain, *cascadiaMono);
    }
    else
    {
        // This stable family name is handed to DirectWrite. If it is unavailable,
        // DirectWrite continues with its normal system fallback rather than
        // failing application startup.
        AddUnique(chain, CascadiaMono);
    }

    if (options.fallbackEnabled)
    {
        AddUnique(chain, SegoeSymbol);
    }
    if (options.emojiFallbackEnabled)
    {
        AddUnique(chain, SegoeEmoji);
    }
    return chain;
}
