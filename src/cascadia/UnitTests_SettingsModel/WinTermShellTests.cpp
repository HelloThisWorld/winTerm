// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include <algorithm>

#include "../../winterm/Clipboard/PasteRiskAnalyzer.h"
#include "../../winterm/Shell/Protocol/ShellIntegrationProtocol.h"
#include "../../winterm/Shell/Sessions/ShellSessionMetadata.h"

using namespace WEX::TestExecution;
using namespace winTerm::Clipboard;
using namespace winTerm::Shell;

namespace SettingsModelUnitTests
{
    class WinTermShellTests
    {
        TEST_CLASS(WinTermShellTests);

        TEST_METHOD(ProtocolClassifierAcceptsKnownSafePayloads);
        TEST_METHOD(PasteRiskAnalyzerClassifiesWithoutChangingText);
    };

    void WinTermShellTests::ProtocolClassifierAcceptsKnownSafePayloads()
    {
        const auto directory = ClassifyShellIntegrationPayload(L"9;9;\"C:\\Projects\\中文\"");
        VERIFY_IS_TRUE(directory.has_value());
        VERIFY_ARE_EQUAL(ShellEventType::CurrentDirectory, directory->type);
        VERIFY_ARE_EQUAL(std::wstring{ L"C:\\Projects\\中文" }, directory->currentDirectory);

        const auto completed = ClassifyShellIntegrationPayload(L"133;D;42");
        VERIFY_IS_TRUE(completed.has_value());
        VERIFY_ARE_EQUAL(ShellEventType::CommandFinished, completed->type);
        VERIFY_ARE_EQUAL(uint32_t{ 42 }, *completed->exitCode);

        VERIFY_IS_FALSE(ClassifyShellIntegrationPayload(L"133;D;not-a-number").has_value());
        VERIFY_IS_FALSE(ClassifyShellIntegrationPayload(std::wstring(MaximumShellProtocolPayloadLength + 1, L'x')).has_value());

        ShellSessionRegistry registry;
        ShellSessionMetadata metadata;
        metadata.sessionId = L"pane-1";
        metadata.currentDirectory = { CurrentDirectoryKind::Remote, L"/remote/app" };
        registry.Upsert(metadata);
        VERIFY_IS_FALSE(registry.Find(L"pane-1")->currentDirectory.IsTrustedLocalPath());
    }

    void WinTermShellTests::PasteRiskAnalyzerClassifiesWithoutChangingText()
    {
        const std::wstring content{ L"Remove-Item -Recurse -Force .\n" };
        const auto analysis = AnalyzePasteRisk(content);

        VERIFY_ARE_EQUAL(content.size(), analysis.characterCount);
        VERIFY_IS_TRUE(analysis.endsWithNewline);
        VERIFY_IS_TRUE(analysis.RequiresConfirmation());
        VERIFY_IS_TRUE(std::find(analysis.reasons.begin(), analysis.reasons.end(), PasteRiskReason::SuspiciousCommand) != analysis.reasons.end());
        VERIFY_ARE_EQUAL(std::wstring{ L"Remove-Item -Recurse -Force .\n" }, content);
    }
}
