# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

function Test-WinTermInteractiveSession
{
    [CmdletBinding()]
    param()

    if ([string]::IsNullOrWhiteSpace($env:WINTERM_SESSION_ID))
    {
        return $false
    }

    if ($Host.Name -ne 'ConsoleHost')
    {
        return $false
    }

    return -not [Console]::IsInputRedirected -and -not [Console]::IsOutputRedirected
}

function Test-WinTermExistingCommand
{
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Name
    )

    $commands = @(Get-Command -Name $Name -All -ErrorAction SilentlyContinue |
        Where-Object { $_.ModuleName -ne 'winTerm.Shell' -and $_.Source -ne 'winTerm.Shell' })
    return $commands.Count -gt 0
}

function Get-WinTermCompatibilityModeInternal
{
    [CmdletBinding()]
    param()

    foreach ($candidate in @($script:WinTermSessionCompatibilityMode, $env:WINTERM_PROFILE_COMPATIBILITY_MODE, $env:WINTERM_COMPATIBILITY_MODE, 'Safe'))
    {
        if ($candidate -in @('Off', 'Safe', 'Extended'))
        {
            return $candidate
        }
    }

    return 'Safe'
}

function Assert-WinTermSafeCompatibilityEnabled
{
    [CmdletBinding()]
    param()

    if ((Get-WinTermCompatibilityModeInternal) -eq 'Off')
    {
        Write-Error -Category InvalidOperation -Message 'winTerm Safe Compatibility mode is disabled for this session.'
        return $false
    }
    return $true
}

function Find-WinTermNativeCommand
{
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Name
    )

    return Get-Command -Name $Name -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
}
