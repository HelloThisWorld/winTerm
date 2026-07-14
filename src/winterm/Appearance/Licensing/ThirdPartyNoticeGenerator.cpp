// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "ThirdPartyNoticeGenerator.h"

#include <array>
#include <sstream>

using namespace winTerm::Appearance::Licensing;

namespace
{
    std::string EscapeMarkdown(const std::string_view value)
    {
        std::string result;
        result.reserve(value.size());
        for (const auto character : value)
        {
            if (character == '\r' || character == '\n')
            {
                result.push_back(' ');
                continue;
            }
            if (character == '\\' || character == '`' || character == '*' || character == '_' ||
                character == '{' || character == '}' || character == '[' || character == ']' ||
                character == '<' || character == '>' || character == '#')
            {
                result.push_back('\\');
            }
            result.push_back(character);
        }
        return result;
    }

    std::string_view Heading(const AssetCategory category) noexcept
    {
        switch (category)
        {
        case AssetCategory::Theme:
            return "Themes";
        case AssetCategory::Font:
            return "Fonts";
        case AssetCategory::InheritedDependency:
            return "Inherited dependencies";
        }
        return "Assets";
    }

    void WriteValue(std::ostringstream& output, const std::string_view label, const std::string& value)
    {
        if (!value.empty())
        {
            output << "- " << label << ": " << EscapeMarkdown(value) << '\n';
        }
    }
}

std::string ThirdPartyNoticeGenerator::Generate(const AssetLicenseRegistry& registry)
{
    std::ostringstream output;
    output << "# Third-Party Notices\n\n"
              "This document is generated from the pinned winTerm asset manifests. "
              "License texts are distributed in the referenced local files.\n\n";

    constexpr std::array categories{
        AssetCategory::Theme,
        AssetCategory::Font,
        AssetCategory::InheritedDependency,
    };
    for (const auto category : categories)
    {
        output << "## " << Heading(category) << "\n\n";
        const auto entries = registry.Entries(category);
        if (entries.empty())
        {
            output << "_No registered entries._\n\n";
            continue;
        }

        for (const auto& entry : entries)
        {
            output << "### " << EscapeMarkdown(entry.name) << "\n\n";
            WriteValue(output, "ID", entry.id);
            WriteValue(output, "Author or project", entry.author);
            WriteValue(output, "Version", entry.version);
            WriteValue(output, "License", entry.license);
            WriteValue(output, "Source project", entry.sourceProject);
            WriteValue(output, "Source revision", entry.sourceRevision);
            WriteValue(output, "Source file", entry.sourceFile);
            WriteValue(output, "Bundled asset", entry.assetFile);
            WriteValue(output, "License file", entry.licenseFile);
            WriteValue(output, "SHA-256", entry.sha256);
            output << '\n';
        }
    }
    return output.str();
}
