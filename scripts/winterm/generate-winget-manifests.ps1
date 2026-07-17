# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^https://github\.com/HelloThisWorld/winTerm/releases/download/v1\.0\.0/winTerm-1\.0\.0-x64\.msix$')]
    [string]$InstallerUrl,

    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$InstallerSha256,

    [Parameter(Mandatory)]
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-Utf8File
{
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string[]]$Lines
    )

    [IO.File]::WriteAllLines($Path, $Lines, [Text.UTF8Encoding]::new($false))
}

try
{
    $output = if ([IO.Path]::IsPathRooted($OutputDirectory))
    {
        [IO.Path]::GetFullPath($OutputDirectory)
    }
    else
    {
        [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
    }
    if (Test-Path -LiteralPath $output)
    {
        throw "WinGet output directory already exists: $output"
    }
    New-Item -ItemType Directory -Path $output | Out-Null
    $sha = $InstallerSha256.ToUpperInvariant()

    Write-Utf8File -Path (Join-Path $output 'Kaname.winTerm.yaml') -Lines @(
        '# Generated only from the public winTerm v1.0.0 GitHub Release.'
        'PackageIdentifier: Kaname.winTerm'
        'PackageVersion: 1.0.0'
        'DefaultLocale: en-US'
        'ManifestType: version'
        'ManifestVersion: 1.6.0'
    )

    Write-Utf8File -Path (Join-Path $output 'Kaname.winTerm.installer.yaml') -Lines @(
        '# Generated only from the public winTerm v1.0.0 GitHub Release.'
        'PackageIdentifier: Kaname.winTerm'
        'PackageVersion: 1.0.0'
        'InstallerType: msix'
        'Scope: user'
        'InstallModes:'
        '- interactive'
        '- silent'
        'UpgradeBehavior: install'
        'Installers:'
        '- Architecture: x64'
        "  InstallerUrl: $InstallerUrl"
        "  InstallerSha256: $sha"
        'ManifestType: installer'
        'ManifestVersion: 1.6.0'
    )

    Write-Utf8File -Path (Join-Path $output 'Kaname.winTerm.locale.en-US.yaml') -Lines @(
        '# Generated only from the public winTerm v1.0.0 GitHub Release.'
        'PackageIdentifier: Kaname.winTerm'
        'PackageVersion: 1.0.0'
        'PackageLocale: en-US'
        'Publisher: winTerm contributors'
        'PublisherUrl: https://github.com/HelloThisWorld/winTerm'
        'PublisherSupportUrl: https://github.com/HelloThisWorld/winTerm/issues'
        'PackageName: winTerm'
        'PackageUrl: https://github.com/HelloThisWorld/winTerm'
        'License: MIT'
        'LicenseUrl: https://github.com/HelloThisWorld/winTerm/blob/v1.0.0/LICENSE'
        'ShortDescription: Independent open-source terminal application based on Microsoft Windows Terminal.'
        'Moniker: winterm'
        'Tags:'
        '- command-line'
        '- console'
        '- terminal'
        '- powershell'
        'ManifestType: defaultLocale'
        'ManifestVersion: 1.6.0'
    )

    Write-Host "Generated WinGet manifests in $output" -ForegroundColor Green
}
catch
{
    Write-Error "WinGet manifest generation failed: $($_.Exception.Message)"
    exit 1
}
