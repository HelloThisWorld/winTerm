// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace winTerm::Clipboard
{
    enum class PasteRiskReason
    {
        MultipleLines,
        TrailingNewline,
        LargeText,
        ControlCharacter,
        CommandSeparator,
        SuspiciousCommand,
    };

    struct PasteRiskSettings
    {
        size_t largePasteCharacterThreshold{ 4096 };
        bool warnForMultiline{ true };
        bool warnForTrailingNewline{ true };
        bool warnForLargePaste{ true };
        bool warnForSuspiciousCommands{ true };
    };

    struct PasteRiskAnalysis
    {
        size_t lineCount{};
        size_t characterCount{};
        bool endsWithNewline{};
        std::vector<PasteRiskReason> reasons;

        bool RequiresConfirmation() const noexcept;
    };

    PasteRiskAnalysis AnalyzePasteRisk(std::wstring_view text, const PasteRiskSettings& settings = {});
}
