// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ThemeImporter.h"

#include <iomanip>
#include <sstream>

using namespace winTerm::Appearance;

std::string ThemeImporter::GenerateImportedId(const std::string_view displayName, const std::string_view content)
{
    std::string slug;
    slug.reserve(std::min<size_t>(displayName.size(), 48));
    bool separatorPending = false;
    for (const unsigned char character : displayName)
    {
        if ((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9'))
        {
            if (separatorPending && !slug.empty())
            {
                slug.push_back('-');
            }
            slug.push_back(static_cast<char>(character));
            separatorPending = false;
        }
        else if (character >= 'A' && character <= 'Z')
        {
            if (separatorPending && !slug.empty())
            {
                slug.push_back('-');
            }
            slug.push_back(static_cast<char>(character + ('a' - 'A')));
            separatorPending = false;
        }
        else
        {
            separatorPending = !slug.empty();
        }
        if (slug.size() >= 48)
        {
            break;
        }
    }
    if (slug.empty())
    {
        slug = "theme";
    }

    uint64_t hash = 14695981039346656037ull;
    for (const unsigned char character : content)
    {
        hash ^= character;
        hash *= 1099511628211ull;
    }
    std::ostringstream result;
    result << "user." << slug << '.' << std::hex << std::setfill('0') << std::setw(12) << (hash & 0xFFFFFFFFFFFFull);
    return result.str();
}

std::string ThemeImporter::LeafName(const std::string_view fileName)
{
    const auto separator = fileName.find_last_of("/\\");
    return std::string{ separator == std::string_view::npos ? fileName : fileName.substr(separator + 1) };
}

std::string ThemeImporter::DisplayNameFromFile(const std::string_view fileName)
{
    auto name = LeafName(fileName);
    constexpr std::array<std::string_view, 4> suffixes{ ".winterm-theme.json", ".itermcolors", ".terminal", ".json" };
    for (const auto suffix : suffixes)
    {
        if (name.size() >= suffix.size())
        {
            const auto offset = name.size() - suffix.size();
            if (std::equal(suffix.begin(), suffix.end(), name.begin() + static_cast<std::ptrdiff_t>(offset), [](const char left, const char right) {
                    return std::tolower(static_cast<unsigned char>(left)) == std::tolower(static_cast<unsigned char>(right));
                }))
            {
                name.resize(offset);
                break;
            }
        }
    }
    return name.empty() ? "Imported Theme" : name;
}

void ThemeImporter::PrepareImportedTheme(ThemeDescriptor& theme, const std::string_view fileName)
{
    theme.source.type = ThemeSourceType::Imported;
    theme.source.project = "Imported theme";
    theme.source.author = "Unknown";
    theme.source.license = "User-provided";
    theme.originalFileName = LeafName(fileName);
}
