# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$')]
    [string]$Version,

    [Parameter()]
    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [string]$StageDirectory,

    [Parameter()]
    [string]$OutputDirectory,

    [Parameter()]
    [switch]$SkipBuild,

    [Parameter()]
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$portableRoot = $null

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$platformLabel = $Platform.ToLowerInvariant()
$stageRoot = if ([string]::IsNullOrWhiteSpace($StageDirectory))
{
    Join-Path $repositoryRoot "artifacts\stage\winTerm-$platformLabel"
}
else
{
    [IO.Path]::GetFullPath($StageDirectory)
}
$outputRoot = if ([string]::IsNullOrWhiteSpace($OutputDirectory))
{
    Join-Path $repositoryRoot 'artifacts\release'
}
else
{
    [IO.Path]::GetFullPath($OutputDirectory)
}
$zipPath = Join-Path $outputRoot "winTerm-$Version-portable-$platformLabel.zip"

try
{
    & (Join-Path $PSScriptRoot 'verify-branding.ps1') -ExpectedPublisher 'CN=helloThisWorld'
    if (-not $?) { throw 'Branding verification failed.' }

    if (-not $SkipBuild -or -not (Test-Path -LiteralPath $stageRoot -PathType Container))
    {
        & (Join-Path $PSScriptRoot 'build-unpackaged.ps1') -Configuration Release -Platform $Platform -OutputDirectory $stageRoot -Force:$Force
        if (-not $?) { throw 'Unpackaged stage build failed.' }
    }
    & (Join-Path $PSScriptRoot 'verify-release-layout.ps1') -Path $stageRoot -Type Stage -Platform $Platform
    if (-not $?) { throw 'Source stage verification failed.' }

    New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
    if (Test-Path -LiteralPath $zipPath)
    {
        if (-not $Force) { throw "Portable artifact already exists: $zipPath" }
        Remove-Item -LiteralPath $zipPath -Force
    }

    $portableRoot = Join-Path ([IO.Path]::GetTempPath()) ("winterm-portable-{0}" -f [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $portableRoot | Out-Null
    Get-ChildItem -LiteralPath $stageRoot -Force | Copy-Item -Destination $portableRoot -Recurse -Force
    [IO.File]::WriteAllText(
        (Join-Path $portableRoot 'portable.marker'),
        "winTerm portable data mode$([Environment]::NewLine)",
        [Text.UTF8Encoding]::new($false))
    $dataRoot = Join-Path $portableRoot 'data'
    New-Item -ItemType Directory -Path $dataRoot | Out-Null
    [IO.File]::WriteAllText(
        (Join-Path $dataRoot 'README.txt'),
        "winTerm stores portable settings, workspaces, snapshots, logs, and updates in this directory.$([Environment]::NewLine)",
        [Text.UTF8Encoding]::new($false))

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($portableRoot, $zipPath, [IO.Compression.CompressionLevel]::Optimal, $false)
    & (Join-Path $PSScriptRoot 'verify-release-layout.ps1') -Path $zipPath -Type Portable -Platform $Platform -Version $Version
    if (-not $?) { throw 'Portable artifact verification failed.' }

    Write-Host "Prepared winTerm Portable ZIP: $zipPath" -ForegroundColor Green
}
catch
{
    Write-Error "Portable build failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    if ($null -ne $portableRoot -and (Test-Path -LiteralPath $portableRoot))
    {
        Remove-Item -LiteralPath $portableRoot -Recurse -Force
    }
}
