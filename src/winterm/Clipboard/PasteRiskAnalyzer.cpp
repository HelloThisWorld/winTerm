// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"
#include "PasteRiskAnalyzer.h"

#include <algorithm>
#include <array>
#include <cwctype>

using namespace winTerm::Clipboard;

namespace
{
    bool Contains(const std::wstring& text, const std::wstring_view value)
    {
        return text.find(value) != std::wstring::npos;
    }

    bool ContainsUnexpectedControlCharacter(const std::wstring_view text)
    {
        return std::any_of(text.begin(), text.end(), [](const wchar_t character) {
            return character == L'\0' || (character < 0x20 && character != L'\r' && character != L'\n' && character != L'\t');
        });
    }

    bool ContainsSuspiciousPattern(const std::wstring& normalized)
    {
        static constexpr std::array<std::wstring_view, 9> patterns{
            L"rm -rf",
            L"remove-item -recurse -force",
            L"format ",
            L"diskpart",
            L"shutdown ",
            L"stop-computer",
            L"del /s /q",
            L"rmdir /s /q",
            L"invoke-expression",
        };

        return std::any_of(patterns.begin(), patterns.end(), [&](const auto pattern) { return Contains(normalized, pattern); });
    }
}

bool PasteRiskAnalysis::RequiresConfirmation() const noexcept
{
    return !reasons.empty();
}

PasteRiskAnalysis winTerm::Clipboard::AnalyzePasteRisk(const std::wstring_view text, const PasteRiskSettings& settings)
{
    PasteRiskAnalysis analysis;
    analysis.characterCount = text.size();
    analysis.lineCount = text.empty() ? 0 : 1 + static_cast<size_t>(std::count(text.begin(), text.end(), L'\n'));
    analysis.endsWithNewline = !text.empty() && (text.back() == L'\n' || text.back() == L'\r');

    if (settings.warnForMultiline && analysis.lineCount > 1)
    {
        analysis.reasons.push_back(PasteRiskReason::MultipleLines);
    }
    if (settings.warnForTrailingNewline && analysis.endsWithNewline)
    {
        analysis.reasons.push_back(PasteRiskReason::TrailingNewline);
    }
    if (settings.warnForLargePaste && analysis.characterCount > settings.largePasteCharacterThreshold)
    {
        analysis.reasons.push_back(PasteRiskReason::LargeText);
    }
    if (ContainsUnexpectedControlCharacter(text))
    {
        analysis.reasons.push_back(PasteRiskReason::ControlCharacter);
    }

    std::wstring normalized{ text };
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](const wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    if (Contains(normalized, L"&&") || Contains(normalized, L"||") || Contains(normalized, L";"))
    {
        analysis.reasons.push_back(PasteRiskReason::CommandSeparator);
    }
    if (settings.warnForSuspiciousCommands && ContainsSuspiciousPattern(normalized))
    {
        analysis.reasons.push_back(PasteRiskReason::SuspiciousCommand);
    }

    return analysis;
}
