# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$')]
    [string]$Version,

    [Parameter(Mandatory)]
    [ValidatePattern('^https://github\.com/HelloThisWorld/winTerm/releases/tag/v.+$')]
    [string]$ReleaseUrl,

    [Parameter(Mandatory)]
    [ValidatePattern('^\d{4}-\d{2}-\d{2}T')]
    [string]$PublishedAt,

    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$SetupX64Sha256,

    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$PortableX64Sha256,

    [Parameter(Mandatory)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try
{
    $expectedReleaseUrl = "https://github.com/HelloThisWorld/winTerm/releases/tag/v$Version"
    if ($ReleaseUrl -cne $expectedReleaseUrl)
    {
        throw "Release URL must match the exact public version: $expectedReleaseUrl"
    }
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
        schemaVersion = 2
        product = 'winTerm'
        productId = 'HelloThisWorld.winTerm'
        publisher = 'helloThisWorld'
        channel = 'stable'
        version = $Version
        tag = "v$Version"
        publishedAt = $timestamp
        releaseUrl = $ReleaseUrl
        artifacts = @(
            [ordered]@{
                architecture = 'x64'
                type = 'inno'
                fileName = "winTerm-$Version-setup-x64.exe"
                sha256 = $SetupX64Sha256.ToLowerInvariant()
            },
            [ordered]@{
                architecture = 'x64'
                type = 'portable-zip'
                fileName = "winTerm-$Version-portable-x64.zip"
                sha256 = $PortableX64Sha256.ToLowerInvariant()
            }
        )
    }
    [IO.File]::WriteAllText(
        $output,
        ($manifest | ConvertTo-Json -Depth 8) + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))
    Write-Host "Generated stable update manifest: $output" -ForegroundColor Green
}
catch
{
    Write-Error "Stable update manifest generation failed: $($_.Exception.Message)"
    exit 1
}
