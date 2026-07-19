# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^https://github\.com/HelloThisWorld/winTerm/releases/tag/v1\.0\.1$')]
    [string]$ReleaseUrl,

    [Parameter(Mandatory)]
    [ValidatePattern('^\d{4}-\d{2}-\d{2}T')]
    [string]$PublishedAt,

    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$X64Sha256,

    [Parameter(Mandatory)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try
{
    $timestamp = [DateTimeOffset]::Parse(
        $PublishedAt,
        [Globalization.CultureInfo]::InvariantCulture,
        [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime().ToString('o')
    $output = if ([IO.Path]::IsPathRooted($OutputPath))
    {
        [IO.Path]::GetFullPath($OutputPath)
    }
    else
    {
        [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
    }
    if (Test-Path -LiteralPath $output)
    {
        throw "Stable update manifest already exists: $output"
    }
    $parent = Split-Path -Parent $output
    if (-not (Test-Path -LiteralPath $parent))
    {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }

    $manifest = [ordered]@{
        schemaVersion = 1
        channel = 'stable'
        version = '1.0.2'
        tag = 'v1.0.2'
        publishedAt = $timestamp
        releaseUrl = $ReleaseUrl
        artifacts = @(
            [ordered]@{
                architecture = 'x64'
                type = 'msix'
                fileName = 'winTerm-1.0.2-x64.msix'
                sha256 = $X64Sha256.ToLowerInvariant()
            }
        )
    }
    $json = $manifest | ConvertTo-Json -Depth 8
    [IO.File]::WriteAllText($output, $json + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
    Write-Host "Generated stable update manifest: $output" -ForegroundColor Green
}
catch
{
    Write-Error "Stable update manifest generation failed: $($_.Exception.Message)"
    exit 1
}
