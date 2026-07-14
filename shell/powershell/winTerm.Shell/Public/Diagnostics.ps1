# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

function Get-WinTermShellDiagnostics
{
    [CmdletBinding()]
    param()

    $psReadLine = Get-Module -Name PSReadLine | Select-Object -First 1
    $shellType = if ($PSVersionTable.PSEdition -eq 'Core') { 'PowerShell' } else { 'Windows PowerShell' }

    return [PSCustomObject]@{
        ShellType = $shellType
        ShellVersion = $PSVersionTable.PSVersion.ToString()
        IntegrationEnabled = $script:WinTermIntegrationEnabled
        IntegrationVersion = $script:WinTermModuleVersion
        ProtocolVersion = $script:WinTermProtocolVersion
        SessionMarkerDetected = -not [string]::IsNullOrWhiteSpace($env:WINTERM_SESSION_ID)
        PromptWrapped = $script:WinTermPromptWrapped
        CurrentDirectoryReporting = if ($script:WinTermIntegrationEnabled) { 'OSC 9;9' } else { 'Disabled' }
        CommandMarks = if ($script:WinTermIntegrationEnabled) { 'OSC 133 A/B/D with upstream autoMarkPrompts' } else { 'Disabled' }
        CompletionProvider = $script:WinTermCompletionProvider
        PSReadLineStatus = if ($null -ne $psReadLine) { 'Loaded' } else { 'Not loaded' }
        CompatibilityMode = Get-WinTermCompatibilityModeInternal
        LastIntegrationError = $script:WinTermLastIntegrationError
    }
}

function Test-WinTermShellIntegration
{
    [CmdletBinding()]
    param()

    $diagnostics = Get-WinTermShellDiagnostics
    return [PSCustomObject]@{
        Healthy = $diagnostics.IntegrationEnabled -and $diagnostics.PromptWrapped -and $diagnostics.SessionMarkerDetected
        Diagnostics = $diagnostics
        Resolution = if ($diagnostics.SessionMarkerDetected) { $null } else { 'Launch this shell from a winTerm profile that sets WINTERM_SESSION_ID.' }
    }
}

function Get-WinTermCompatibilityMode
{
    [CmdletBinding()]
    param()

    return Get-WinTermCompatibilityModeInternal
}

function Set-WinTermCompatibilityMode
{
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [ValidateSet('Off', 'Safe', 'Extended')]
        [string]$Mode,

        [ValidateSet('Session', 'Profile', 'Global')]
        [string]$Scope = 'Session'
    )

    switch ($Scope)
    {
        'Session'
        {
            $script:WinTermSessionCompatibilityMode = $Mode
        }
        'Profile'
        {
            $env:WINTERM_PROFILE_COMPATIBILITY_MODE = $Mode
        }
        'Global'
        {
            $env:WINTERM_COMPATIBILITY_MODE = $Mode
        }
    }

    return Get-WinTermCompatibilityModeInternal
}
