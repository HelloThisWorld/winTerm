// Copyright (c) winTerm contributors.
// Licensed under the MIT license.

#include "pch.h"

#include <algorithm>

#include "../../winterm/Clipboard/PasteRiskAnalyzer.h"
#include "../../winterm/Shell/AutoIntegration.h"
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
        TEST_METHOD(AutoIntegrationRewritesBarePowerShellCommandlines);
        TEST_METHOD(AutoIntegrationRefusesCustomizedOrUnsafeInput);
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

    void WinTermShellTests::AutoIntegrationRewritesBarePowerShellCommandlines()
    {
        const std::wstring manifest{ L"C:\\Program Files\\winTerm\\ShellAssets\\powershell\\winTerm.Shell\\winTerm.Shell.psd1" };
        const std::wstring sessionId{ L"{01234567-89ab-cdef-0123-456789abcdef}" };

        const auto stock = BuildAutoIntegratedPowerShellCommandline(
            L"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe", manifest, sessionId);
        VERIFY_IS_TRUE(stock.has_value());
        if (stock)
        {
            // The original invocation is preserved as the prefix, and the
            // appended fragment sets the session marker before the import.
            VERIFY_ARE_EQUAL(size_t{ 0 }, stock->find(L"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"));
            VERIFY_IS_TRUE(stock->find(L"-NoExit -Command") != std::wstring::npos);
            const auto marker = stock->find(L"$env:WINTERM_SESSION_ID='" + sessionId + L"'");
            const auto version = stock->find(L"$env:WINTERM_INTEGRATION_VERSION='1'");
            const auto import = stock->find(L"Import-Module -Name '" + manifest + L"' -ErrorAction SilentlyContinue");
            VERIFY_IS_TRUE(marker != std::wstring::npos);
            VERIFY_IS_TRUE(version != std::wstring::npos);
            VERIFY_IS_TRUE(import != std::wstring::npos);
            VERIFY_IS_TRUE(marker < version && version < import);
        }

        const auto quoted = BuildAutoIntegratedPowerShellCommandline(
            L"\"C:\\Program Files\\PowerShell\\7\\pwsh.exe\" -NoLogo", manifest, sessionId);
        VERIFY_IS_TRUE(quoted.has_value());

        const auto bareName = BuildAutoIntegratedPowerShellCommandline(L"pwsh -NoExit", manifest, sessionId);
        VERIFY_IS_TRUE(bareName.has_value());

        // A rewritten commandline reused by a restarted connection is not
        // rewritten a second time.
        if (stock)
        {
            VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(*stock, manifest, sessionId).has_value());
        }
    }

    void WinTermShellTests::AutoIntegrationRefusesCustomizedOrUnsafeInput()
    {
        const std::wstring manifest{ L"C:\\winTerm\\ShellAssets\\powershell\\winTerm.Shell\\winTerm.Shell.psd1" };
        const std::wstring sessionId{ L"session-1" };

        // Only PowerShell hosts are recognized, by executable basename.
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"%SystemRoot%\\System32\\cmd.exe", manifest, sessionId).has_value());
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"wsl.exe", manifest, sessionId).has_value());
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"C:\\tools\\notpowershell.exe", manifest, sessionId).has_value());
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"powershell.exe.bat", manifest, sessionId).has_value());

        // Any argument beyond -NoLogo and -NoExit means a customized
        // invocation, which is never rewritten.
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"powershell.exe -Command \"Get-Date\"", manifest, sessionId).has_value());
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"powershell.exe -File demo.ps1", manifest, sessionId).has_value());
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"powershell.exe -ExecutionPolicy Bypass", manifest, sessionId).has_value());
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"pwsh.exe -NoProfile", manifest, sessionId).has_value());

        // Quotes inside the manifest path or the session id could break out
        // of the injected fragment, so both are refused outright.
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"pwsh.exe", L"C:\\odd'path\\winTerm.Shell.psd1", sessionId).has_value());
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"pwsh.exe", manifest, L"bad'id").has_value());
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"pwsh.exe", manifest, L"bad id").has_value());
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"pwsh.exe", {}, sessionId).has_value());
        VERIFY_IS_FALSE(BuildAutoIntegratedPowerShellCommandline(L"pwsh.exe", manifest, {}).has_value());
    }
}
