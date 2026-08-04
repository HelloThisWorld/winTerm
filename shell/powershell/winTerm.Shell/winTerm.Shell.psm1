# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

Set-StrictMode -Version Latest

$script:WinTermModuleVersion = '1.3.0'
$script:WinTermProtocolVersion = 1
$script:WinTermIntegrationEnabled = $false
$script:WinTermPromptWrapped = $false
$script:WinTermHasPrompted = $false
$script:WinTermOriginalPrompt = $null
$script:WinTermPromptWrapper = $null
$script:WinTermSessionCompatibilityMode = $null
$script:WinTermLastIntegrationError = $null
$script:WinTermCompletionProvider = 'PowerShell native completion'

# Shell integration must survive a blocked component file. Some antivirus
# engines block individual script files at parse time; a failed dot-source is
# recorded and that component is skipped, instead of spilling a parse error
# into the user's session. Integration itself needs only the Private files.
$script:WinTermFailedComponents = @()
foreach ($relativePath in @(
        'Private\State.ps1',
        'Private\Protocol.ps1',
        'Private\Prompt.ps1',
        'Public\Diagnostics.ps1',
        'Public\Compatibility.ps1',
        'Completion\CompatibilityCompletion.ps1'
    ))
{
    try
    {
        . (Join-Path $PSScriptRoot $relativePath) 2>$null
    }
    catch
    {
        $script:WinTermFailedComponents += $relativePath
        $script:WinTermLastIntegrationError = 'A module component was blocked or failed to load and was skipped.'
    }
}

# Only functions that actually loaded may be exported or invoked, so a
# skipped component degrades that one capability and nothing else.
function Test-WinTermModuleFunction
{
    param(
        [Parameter(Mandatory)]
        [string]$Name
    )

    return $null -ne (Get-Command -Name $Name -CommandType Function -ErrorAction SilentlyContinue)
}

$script:WinTermExportedCompatibilityCommands = @()
if (Test-WinTermModuleFunction -Name 'Test-WinTermExistingCommand')
{
    foreach ($commandName in @('ll', 'la', 'which', 'touch', 'open'))
    {
        if ((Test-WinTermModuleFunction -Name $commandName) -and
            -not (Test-WinTermExistingCommand -Name $commandName))
        {
            $script:WinTermExportedCompatibilityCommands += $commandName
        }
    }
}

if (Test-WinTermModuleFunction -Name 'Register-WinTermCompatibilityCompletion')
{
    Register-WinTermCompatibilityCompletion
}

if ((Test-WinTermModuleFunction -Name 'Test-WinTermInteractiveSession') -and
    (Test-WinTermModuleFunction -Name 'Enable-WinTermShellIntegration') -and
    (Test-WinTermInteractiveSession))
{
    Enable-WinTermShellIntegration | Out-Null
}

$ExecutionContext.SessionState.Module.OnRemove = {
    if ($null -ne (Get-Command -Name 'Disable-WinTermShellIntegration' -CommandType Function -ErrorAction SilentlyContinue))
    {
        Disable-WinTermShellIntegration | Out-Null
    }
}

$script:WinTermExportedFunctions = @()
foreach ($functionName in @(
        'Get-WinTermShellDiagnostics',
        'Test-WinTermShellIntegration',
        'Enable-WinTermShellIntegration',
        'Disable-WinTermShellIntegration',
        'Get-WinTermCompatibilityMode',
        'Set-WinTermCompatibilityMode'
    ))
{
    if (Test-WinTermModuleFunction -Name $functionName)
    {
        $script:WinTermExportedFunctions += $functionName
    }
}

Export-ModuleMember -Function ($script:WinTermExportedFunctions + $script:WinTermExportedCompatibilityCommands) -Variable @()
