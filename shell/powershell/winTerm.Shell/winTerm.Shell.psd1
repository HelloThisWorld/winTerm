@{
    RootModule = 'winTerm.Shell.psm1'
    ModuleVersion = '1.0.2'
    GUID = 'f65cd8f4-5d25-4a2a-a0d4-58df1ab3dc5a'
    Author = 'winTerm contributors'
    CompanyName = 'winTerm'
    Copyright = '(c) winTerm contributors. Licensed under the MIT license.'
    Description = 'Shell integration and safe command compatibility for winTerm.'
    PowerShellVersion = '5.1'
    FunctionsToExport = @('Get-WinTermShellDiagnostics', 'Test-WinTermShellIntegration', 'Enable-WinTermShellIntegration', 'Disable-WinTermShellIntegration', 'Get-WinTermCompatibilityMode', 'Set-WinTermCompatibilityMode', 'll', 'la', 'which', 'touch', 'open')
    CmdletsToExport = @()
    AliasesToExport = @()
    PrivateData = @{
        PSData = @{
            Tags = @('winTerm', 'terminal', 'shell-integration')
            ProjectUri = 'https://github.com/HelloThisWorld/winTerm'
            LicenseUri = 'https://github.com/HelloThisWorld/winTerm/blob/main/LICENSE'
        }
    }
}
