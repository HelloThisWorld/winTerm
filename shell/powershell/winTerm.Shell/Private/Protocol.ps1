# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

# Returns the complete escape sequence for a payload, or an empty string when
# the payload is not eligible. The string terminator is ESC followed by one
# backslash; in PowerShell single quotes a backslash is already literal, so
# '\' is exactly one character.
function Get-WinTermOscSequence
{
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Payload
    )

    if ($Payload.Length -eq 0 -or $Payload.Length -gt 8192 -or $Payload -match '[\x00-\x1F\x7F]')
    {
        $script:WinTermLastIntegrationError = 'An invalid shell integration payload was ignored.'
        return ''
    }

    return ([char]27).ToString() + ']' + $Payload + [char]27 + '\'
}

function Write-WinTermOsc
{
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string]$Payload
    )

    $sequence = Get-WinTermOscSequence -Payload $Payload
    if ($sequence.Length -eq 0)
    {
        return
    }

    try
    {
        [Console]::Out.Write($sequence)
    }
    catch
    {
        $script:WinTermLastIntegrationError = 'The terminal did not accept a shell integration sequence.'
    }
}

# Returns the current-directory sequence, or an empty string when the current
# location is not an eligible file-system path.
function Get-WinTermCurrentDirectorySequence
{
    [CmdletBinding()]
    param()

    try
    {
        $location = Get-Location
        if ($location.Provider.Name -ne 'FileSystem')
        {
            return ''
        }

        $path = $location.ProviderPath
        if ([string]::IsNullOrWhiteSpace($path) -or $path -match '[\x00-\x1F\x7F"]')
        {
            return ''
        }

        return Get-WinTermOscSequence -Payload ('9;9;"' + $path + '"')
    }
    catch
    {
        $script:WinTermLastIntegrationError = 'The current directory could not be reported.'
        return ''
    }
}

function Send-WinTermCurrentDirectory
{
    [CmdletBinding()]
    param()

    $sequence = Get-WinTermCurrentDirectorySequence
    if ($sequence.Length -eq 0)
    {
        return
    }

    try
    {
        [Console]::Out.Write($sequence)
    }
    catch
    {
        $script:WinTermLastIntegrationError = 'The terminal did not accept a shell integration sequence.'
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
