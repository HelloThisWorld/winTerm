# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

function Invoke-WinTermList
{
    [CmdletBinding()]
    param(
        [string[]]$ArgumentList,

        [Parameter(Mandatory)]
        [string]$CommandName
    )

    if (-not (Assert-WinTermSafeCompatibilityEnabled))
    {
        return
    }

    $native = Find-WinTermNativeCommand -Name $CommandName
    if ($null -ne $native)
    {
        & $native.Source @ArgumentList
        return
    }

    $path = '.'
    foreach ($argument in @($ArgumentList))
    {
        if ($argument -in @('-a', '--all'))
        {
            continue
        }
        if ($argument.StartsWith('-'))
        {
            Write-Error -Category InvalidArgument -Message "Unsupported option '$argument'. Supported options are -a and --all."
            return
        }
        if ($path -ne '.')
        {
            Write-Error -Category InvalidArgument -Message 'll and la accept one path argument.'
            return
        }
        $path = $argument
    }

    Get-ChildItem -LiteralPath $path -Force
}

function ll
{
    <#
    .SYNOPSIS
    Lists a directory including hidden items when Safe Compatibility mode is enabled.
    .EXAMPLE
    ll C:\Projects
    #>
    [CmdletBinding()]
    param(
        [Parameter(ValueFromRemainingArguments = $true, Position = 0)]
        [string[]]$ArgumentList
    )

    Invoke-WinTermList -ArgumentList $ArgumentList -CommandName 'll'
}

function la
{
    <#
    .SYNOPSIS
    Lists a directory including hidden items when Safe Compatibility mode is enabled.
    .EXAMPLE
    la --all
    #>
    [CmdletBinding()]
    param(
        [Parameter(ValueFromRemainingArguments = $true, Position = 0)]
        [string[]]$ArgumentList
    )

    Invoke-WinTermList -ArgumentList $ArgumentList -CommandName 'la'
}

function which
{
    <#
    .SYNOPSIS
    Displays the command type and source for a command.
    .EXAMPLE
    which git
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory, Position = 0)]
        [ValidateNotNullOrEmpty()]
        [string]$Name
    )

    if (-not (Assert-WinTermSafeCompatibilityEnabled))
    {
        return
    }

    $native = Find-WinTermNativeCommand -Name 'which'
    if ($null -ne $native)
    {
        & $native.Source $Name
        return
    }

    $command = Get-Command -Name $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $command)
    {
        Write-Error -Category ObjectNotFound -Message "Command '$Name' was not found."
        return
    }

    $source = if (-not [string]::IsNullOrWhiteSpace($command.Source)) { $command.Source } elseif (-not [string]::IsNullOrWhiteSpace($command.Path)) { $command.Path } else { $command.Definition }
    [PSCustomObject]@{
        Name = $command.Name
        CommandType = $command.CommandType.ToString()
        Source = $source
    }
}

function touch
{
    <#
    .SYNOPSIS
    Creates an empty file or updates the last-write time without truncating content.
    .EXAMPLE
    touch notes.txt
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory, ValueFromRemainingArguments = $true, Position = 0)]
        [ValidateNotNullOrEmpty()]
        [string[]]$Path
    )

    if (-not (Assert-WinTermSafeCompatibilityEnabled))
    {
        return
    }

    $native = Find-WinTermNativeCommand -Name 'touch'
    if ($null -ne $native)
    {
        & $native.Source @Path
        return
    }

    foreach ($candidate in $Path)
    {
        $uri = $null
        if ([Uri]::TryCreate($candidate, [UriKind]::Absolute, [ref]$uri) -and -not $uri.IsFile)
        {
            Write-Error -Category InvalidArgument -Message "touch accepts file system paths only: '$candidate'."
            continue
        }

        try
        {
            $fileSystemPath = [System.IO.Path]::GetFullPath($candidate)
        }
        catch
        {
            Write-Error -Category InvalidArgument -Message "touch accepts valid file system paths only: '$candidate'."
            continue
        }

        if (Test-Path -LiteralPath $fileSystemPath)
        {
            $item = Get-Item -LiteralPath $fileSystemPath -Force
            $item.LastWriteTime = Get-Date
            continue
        }

        $parent = Split-Path -Path $fileSystemPath -Parent
        if (-not [string]::IsNullOrWhiteSpace($parent) -and -not (Test-Path -LiteralPath $parent -PathType Container))
        {
            Write-Error -Category ObjectNotFound -Message "The parent directory for '$candidate' does not exist."
            continue
        }

        try
        {
            $stream = [System.IO.File]::Open($fileSystemPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::Write, [System.IO.FileShare]::ReadWrite)
            $stream.Dispose()
        }
        catch
        {
            Write-Error -Category WriteError -Message "The file '$candidate' could not be created."
        }
    }
}

function open
{
    <#
    .SYNOPSIS
    Opens an existing file system target or HTTP(S) URL through its registered Windows handler.
    .EXAMPLE
    open .
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory, Position = 0)]
        [ValidateNotNullOrEmpty()]
        [string]$Target
    )

    if (-not (Assert-WinTermSafeCompatibilityEnabled))
    {
        return
    }

    $native = Find-WinTermNativeCommand -Name 'open'
    if ($null -ne $native)
    {
        & $native.Source $Target
        return
    }

    $uri = $null
    if ([Uri]::TryCreate($Target, [UriKind]::Absolute, [ref]$uri) -and -not $uri.IsFile)
    {
        if ($uri.Scheme -notin @('http', 'https'))
        {
            Write-Error -Category InvalidArgument -Message "The URL scheme '$($uri.Scheme)' is not supported."
            return
        }
        Invoke-Item -Path $uri.AbsoluteUri
        return
    }

    if (-not (Test-Path -LiteralPath $Target))
    {
        Write-Error -Category ObjectNotFound -Message "The target '$Target' does not exist."
        return
    }

    Invoke-Item -LiteralPath $Target
}
