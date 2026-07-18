# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Directory,

    [Parameter()]
    [string]$ChecksumFile = 'SHA256SUMS.txt'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try
{
    $root = (Resolve-Path -LiteralPath $Directory).Path
    $checksumPath = Join-Path $root $ChecksumFile
    if (-not (Test-Path -LiteralPath $checksumPath -PathType Leaf))
    {
        throw "Checksum file is missing: $checksumPath"
    }

    $seen = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $checksumPath)
    {
        $lineNumber++
        if ($line -notmatch '^(?<hash>[0-9a-f]{64})  (?<name>[^\\/]+)$')
        {
            throw "Invalid checksum syntax at line $lineNumber."
        }

        $name = $Matches.name
        if ($name -eq $ChecksumFile -or -not $seen.Add($name))
        {
            throw "Duplicate or recursive checksum entry '$name'."
        }

        $path = Join-Path $root $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf))
        {
            throw "Checksummed file is missing: $name"
        }
        $item = Get-Item -LiteralPath $path
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)
        {
            throw "Checksummed file '$name' is a reparse point."
        }

        $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -cne $Matches.hash)
        {
            throw "SHA-256 mismatch for '$name'."
        }
        Write-Host "PASS: $name" -ForegroundColor Green
    }

    if ($seen.Count -eq 0)
    {
        throw 'Checksum file contains no entries.'
    }
    Write-Host "Verified $($seen.Count) SHA-256 checksum(s)." -ForegroundColor Green
}
catch
{
    Write-Error "Checksum verification failed: $($_.Exception.Message)"
    exit 1
}
