# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Directory,

    [Parameter()]
    [string]$OutputFile = 'SHA256SUMS.txt'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try
{
    $root = (Resolve-Path -LiteralPath $Directory).Path
    $outputPath = Join-Path $root $OutputFile
    $files = @(Get-ChildItem -LiteralPath $root -File |
        Where-Object { $_.Name -ne $OutputFile } |
        Sort-Object Name)

    if ($files.Count -eq 0)
    {
        throw "No files were found in '$root'."
    }

    $lines = foreach ($file in $files)
    {
        if (($file.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)
        {
            throw "Release file '$($file.Name)' is a reparse point."
        }
        if ($file.Length -le 0)
        {
            throw "Release file '$($file.Name)' is empty."
        }
        "$((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant())  $($file.Name)"
    }

    [System.IO.File]::WriteAllLines($outputPath, $lines, [System.Text.Encoding]::ASCII)
    Write-Host "Generated SHA-256 checksums: $outputPath" -ForegroundColor Green
}
catch
{
    Write-Error "Checksum generation failed: $($_.Exception.Message)"
    exit 1
}
