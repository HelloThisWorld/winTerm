# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$')]
    [string]$Version,

    [Parameter(Mandatory)]
    [ValidatePattern('^https://github\.com/HelloThisWorld/winTerm/releases/download/.+\.exe$')]
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
    $expectedUrl = "https://github.com/HelloThisWorld/winTerm/releases/download/v$Version/winTerm-$Version-setup-x64.exe"
    if ($InstallerUrl -cne $expectedUrl)
    {
        throw "Installer URL must identify the exact public winTerm $Version Setup EXE: $expectedUrl"
    }

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
    $generatedComment = "# Generated only from the public winTerm v$Version GitHub Release."

    Write-Utf8File -Path (Join-Path $output 'HelloThisWorld.winTerm.yaml') -Lines @(
        '# yaml-language-server: $schema=https://aka.ms/winget-manifest.version.1.12.0.schema.json'
        $generatedComment
        'PackageIdentifier: HelloThisWorld.winTerm'
        "PackageVersion: $Version"
        'DefaultLocale: en-US'
        'ManifestType: version'
        'ManifestVersion: 1.12.0'
    )

    Write-Utf8File -Path (Join-Path $output 'HelloThisWorld.winTerm.installer.yaml') -Lines @(
        '# yaml-language-server: $schema=https://aka.ms/winget-manifest.installer.1.12.0.schema.json'
        $generatedComment
        'PackageIdentifier: HelloThisWorld.winTerm'
        "PackageVersion: $Version"
        'InstallerType: inno'
        'InstallerLocale: en-US'
        'MinimumOSVersion: 10.0.22000.0'
        'InstallModes:'
        '- interactive'
        '- silent'
        '- silentWithProgress'
        'UpgradeBehavior: install'
        'Commands:'
        '- winterm'
        'AppsAndFeaturesEntries:'
        '- DisplayName: winTerm'
        '  Publisher: helloThisWorld'
        '  ProductCode: "{0B9D8F09-5F04-4E9C-879F-14B65E3A8917}_is1"'
        '  InstallerType: inno'
        'Installers:'
        '- Architecture: x64'
        '  Scope: user'
        "  InstallerUrl: $InstallerUrl"
        "  InstallerSha256: $sha"
        '  InstallerSwitches:'
        '    Custom: /CURRENTUSER'
        '- Architecture: x64'
        '  Scope: machine'
        "  InstallerUrl: $InstallerUrl"
        "  InstallerSha256: $sha"
        '  InstallerSwitches:'
        '    Custom: /ALLUSERS'
        'ManifestType: installer'
        'ManifestVersion: 1.12.0'
    )

    Write-Utf8File -Path (Join-Path $output 'HelloThisWorld.winTerm.locale.en-US.yaml') -Lines @(
        '# yaml-language-server: $schema=https://aka.ms/winget-manifest.defaultLocale.1.12.0.schema.json'
        $generatedComment
        'PackageIdentifier: HelloThisWorld.winTerm'
        "PackageVersion: $Version"
        'PackageLocale: en-US'
        'Publisher: helloThisWorld'
        'PublisherUrl: https://github.com/HelloThisWorld'
        'PublisherSupportUrl: https://github.com/HelloThisWorld/winTerm/issues'
        'PackageName: winTerm'
        'PackageUrl: https://github.com/HelloThisWorld/winTerm'
        'License: MIT'
        "LicenseUrl: https://github.com/HelloThisWorld/winTerm/blob/v$Version/LICENSE"
        'Copyright: Copyright (c) winTerm contributors'
        'ShortDescription: Independent open-source terminal application based on Microsoft Terminal.'
        'Description: A Windows 11 terminal with directed splits, pane controls, docking, themes, and an unpackaged self-contained runtime.'
        'Moniker: winterm'
        'Tags:'
        '- command-line'
        '- console'
        '- terminal'
        '- powershell'
        'ManifestType: defaultLocale'
        'ManifestVersion: 1.12.0'
    )

    Write-Host "Generated WinGet $Version Inno manifests in $output" -ForegroundColor Green
}
catch
{
    Write-Error "WinGet manifest generation failed: $($_.Exception.Message)"
    exit 1
}
