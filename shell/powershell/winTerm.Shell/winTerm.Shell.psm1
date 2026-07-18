# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

Set-StrictMode -Version Latest

$script:WinTermModuleVersion = '1.0.1'
$script:WinTermProtocolVersion = 1
$script:WinTermIntegrationEnabled = $false
$script:WinTermPromptWrapped = $false
$script:WinTermHasPrompted = $false
$script:WinTermOriginalPrompt = $null
$script:WinTermPromptWrapper = $null
$script:WinTermSessionCompatibilityMode = $null
$script:WinTermLastIntegrationError = $null
$script:WinTermCompletionProvider = 'PowerShell native completion'

foreach ($relativePath in @(
        'Private\State.ps1',
        'Private\Protocol.ps1',
        'Private\Prompt.ps1',
        'Public\Diagnostics.ps1',
        'Public\Compatibility.ps1',
        'Completion\CompatibilityCompletion.ps1'
    ))
{
    . (Join-Path $PSScriptRoot $relativePath)
}

$script:WinTermExportedCompatibilityCommands = @()
foreach ($commandName in @('ll', 'la', 'which', 'touch', 'open'))
{
    if (-not (Test-WinTermExistingCommand -Name $commandName))
    {
        $script:WinTermExportedCompatibilityCommands += $commandName
    }
}

Register-WinTermCompatibilityCompletion

if (Test-WinTermInteractiveSession)
{
    Enable-WinTermShellIntegration | Out-Null
}

$ExecutionContext.SessionState.Module.OnRemove = {
    Disable-WinTermShellIntegration | Out-Null
}

Export-ModuleMember -Function @(
    'Get-WinTermShellDiagnostics',
    'Test-WinTermShellIntegration',
    'Enable-WinTermShellIntegration',
    'Disable-WinTermShellIntegration',
    'Get-WinTermCompatibilityMode',
    'Set-WinTermCompatibilityMode'
) -Variable @()

if ($script:WinTermExportedCompatibilityCommands.Count -gt 0)
{
    Export-ModuleMember -Function $script:WinTermExportedCompatibilityCommands -Variable @()
}
