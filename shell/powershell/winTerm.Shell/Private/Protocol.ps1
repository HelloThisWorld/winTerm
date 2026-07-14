# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

function Write-WinTermOsc
{
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Payload
    )

    if ($Payload.Length -eq 0 -or $Payload.Length -gt 8192 -or $Payload -match '[\x00-\x1F\x7F]')
    {
        $script:WinTermLastIntegrationError = 'An invalid shell integration payload was ignored.'
        return
    }

    try
    {
        [Console]::Out.Write((([char]27).ToString() + ']' + $Payload + [char]27 + '\\'))
    }
    catch
    {
        $script:WinTermLastIntegrationError = 'The terminal did not accept a shell integration sequence.'
    }
}

function Send-WinTermCurrentDirectory
{
    [CmdletBinding()]
    param()

    try
    {
        $location = Get-Location
        if ($location.Provider.Name -ne 'FileSystem')
        {
            return
        }

        $path = $location.ProviderPath
        if ([string]::IsNullOrWhiteSpace($path) -or $path -match '[\x00-\x1F\x7F"]')
        {
            return
        }

        Write-WinTermOsc -Payload ('9;9;"' + $path + '"')
    }
    catch
    {
        $script:WinTermLastIntegrationError = 'The current directory could not be reported.'
    }
}

function Get-WinTermExitCode
{
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [bool]$LastSuccess
    )

    if ($LastSuccess)
    {
        return 0
    }

    if ($null -ne $global:LASTEXITCODE -and [int]$global:LASTEXITCODE -ne 0)
    {
        return [int]$global:LASTEXITCODE
    }

    return 1
}
