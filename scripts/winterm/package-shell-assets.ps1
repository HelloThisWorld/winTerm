# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [string]$PackageRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourceAssets = @(
    'shell\shared\version.json',
    'shell\powershell\winTerm.Shell\winTerm.Shell.psd1',
    'shell\powershell\winTerm.Shell\winTerm.Shell.psm1',
    'shell\cmd\winterm-init.cmd',
    'shell\cmd\winterm-doskey.cmd',
    'shell\cmd\winterm-prompt.cmd',
    'src\winterm-tools\winterm-shim\main.cpp',
    'src\winterm-tools\winterm-shim\winterm-shim.vcxproj'
)

foreach ($relativePath in $sourceAssets)
{
    $path = Join-Path $repositoryRoot $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf))
    {
        throw "Required shell asset '$relativePath' is missing."
    }
}

$version = Get-Content -LiteralPath (Join-Path $repositoryRoot 'shell\shared\version.json') -Raw | ConvertFrom-Json
if ($version.moduleVersion -ne '1.0.2' -or $version.applicationVersion -ne '1.0.2' -or $version.protocolVersion -ne 1)
{
    throw 'The winTerm Shell asset version metadata is invalid.'
}

if ([string]::IsNullOrWhiteSpace($PackageRoot))
{
    Write-Host 'winTerm Shell source assets are complete.' -ForegroundColor Green
    return
}

$packageAssets = @(
    'ShellAssets\version.json',
    'ShellAssets\powershell\winTerm.Shell\winTerm.Shell.psd1',
    'ShellAssets\powershell\winTerm.Shell\winTerm.Shell.psm1',
    'ShellAssets\cmd\winterm-init.cmd',
    'ShellAssets\cmd\winterm-doskey.cmd',
    'ShellAssets\cmd\winterm-prompt.cmd',
    'ShellAssets\winterm-shim.exe'
)
foreach ($relativePath in $packageAssets)
{
    if (-not (Test-Path -LiteralPath (Join-Path $PackageRoot $relativePath) -PathType Leaf))
    {
        throw "The package is missing shell asset '$relativePath'."
    }
}

Write-Host 'winTerm Shell package assets are complete.' -ForegroundColor Green
