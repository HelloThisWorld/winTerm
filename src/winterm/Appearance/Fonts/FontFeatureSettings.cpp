// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "FontFeatureSettings.h"

#include <cmath>

using namespace winTerm::Appearance;

namespace
{
    void AddIssue(FontFeatureValidationResult& result, std::string field, std::string message)
    {
        result.issues.emplace_back(FontFeatureValidationIssue{ std::move(field), std::move(message) });
    }
}

bool FontFeatureValidationResult::IsValid() const noexcept
{
    return issues.empty();
}

std::vector<std::string> FontFeatureValidationResult::ErrorMessages() const
{
    std::vector<std::string> messages;
    messages.reserve(issues.size());
    for (const auto& issue : issues)
    {
        messages.emplace_back(issue.field.empty() ? issue.message : issue.field + ": " + issue.message);
    }
    return messages;
}

FontFeatureValidationResult FontFeatureSettings::Validate() const
{
    FontFeatureValidationResult result;
    if (!std::isfinite(size) || size < MinimumFontSize || size > MaximumFontSize)
    {
        AddIssue(result, "size", "Font size must be a finite number from 1.0 through 256.0.");
    }
    if (weight < MinimumFontWeight || weight > MaximumFontWeight)
    {
        AddIssue(result, "weight", "Font weight must be from 1 through 1000.");
    }
    if (!std::isfinite(lineHeight) || lineHeight < MinimumLineHeight || lineHeight > MaximumLineHeight)
    {
        AddIssue(result, "lineHeight", "Line height must be a finite number from 0.5 through 4.0.");
    }
    if (!std::isfinite(cellWidthAdjustment) ||
        cellWidthAdjustment < MinimumCellWidthAdjustment ||
        cellWidthAdjustment > MaximumCellWidthAdjustment)
    {
        AddIssue(result, "cellWidthAdjustment", "Cell width adjustment must be a finite number from -0.5 through 1.0.");
    }
    return result;
}
