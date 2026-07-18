# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [switch]$SkipBuild,

    [Parameter()]
    [ValidatePattern('^CN=')]
    [string]$ExpectedPublisher = 'CN=winTerm Development'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-WinTermManifest
{
    param(
        [Parameter(Mandatory)]
    [string]$Path,

        [Parameter(Mandatory)]
        [string]$Publisher
    )

    [xml]$manifest = Get-Content -LiteralPath $Path -Raw
    $namespaceManager = New-Object System.Xml.XmlNamespaceManager($manifest.NameTable)
    $namespaceManager.AddNamespace('f', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
    $namespaceManager.AddNamespace('uap3', 'http://schemas.microsoft.com/appx/manifest/uap/windows10/3')

    $identity = $manifest.SelectSingleNode('/f:Package/f:Identity', $namespaceManager)
    if ($null -eq $identity -or $identity.Name -ne 'HelloThisWorld.winTerm')
    {
        throw "Package '$Path' does not use the HelloThisWorld.winTerm identity."
    }
    if ($identity.Publisher -match 'Microsoft' -or $identity.Publisher -cne $Publisher)
    {
        throw "Package '$Path' does not use the expected non-Microsoft publisher identity."
    }

    $aliases = @($manifest.SelectNodes('//uap3:AppExecutionAlias/*', $namespaceManager) | ForEach-Object { $_.Alias })
    if ($aliases -notcontains 'winterm.exe' -or $aliases -contains 'wt.exe')
    {
        throw "Package '$Path' must claim winterm.exe and must not claim wt.exe."
    }
    if ($identity.Version -ne '1.0.1.0')
    {
        throw "Package '$Path' must use the winTerm 1.0.1.0 package version."
    }
}

function Assert-AppearancePayload
{
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$RepositoryRoot
    )

    $appearanceRoot = Join-Path $Path 'AppearanceAssets'
    $themeManifestPath = Join-Path $appearanceRoot 'themes\manifest.json'
    $fontManifestPath = Join-Path $appearanceRoot 'fonts\manifest.json'
    $licenseIndexPath = Join-Path $appearanceRoot 'licenses\open-source-licenses.html'
    $noticesPath = Join-Path $Path 'THIRD_PARTY_NOTICES.md'
    $inheritedNoticesPath = Join-Path $Path 'NOTICE.md'
    $projectLicensePath = Join-Path $Path 'LICENSE'
    foreach ($requiredPath in @($themeManifestPath, $fontManifestPath, $licenseIndexPath, $noticesPath, $inheritedNoticesPath, $projectLicensePath))
    {
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf))
        {
            throw "The package is missing required appearance payload '$requiredPath'."
        }
    }
    foreach ($noticeName in @('LICENSE', 'NOTICE.md', 'THIRD_PARTY_NOTICES.md'))
    {
        $sourceNoticePath = Join-Path $RepositoryRoot $noticeName
        $packagedNoticePath = Join-Path $Path $noticeName
        $sourceHash = (Get-FileHash -LiteralPath $sourceNoticePath -Algorithm SHA256).Hash
        $packagedHash = (Get-FileHash -LiteralPath $packagedNoticePath -Algorithm SHA256).Hash
        if ($sourceHash -cne $packagedHash)
        {
            throw "The packaged notice '$noticeName' does not match the repository source."
        }
    }

    $themeManifest = Get-Content -LiteralPath $themeManifestPath -Raw | ConvertFrom-Json
    foreach ($theme in $themeManifest.themes)
    {
        $themePath = Join-Path (Split-Path $themeManifestPath) $theme.file
        if (-not (Test-Path -LiteralPath $themePath -PathType Leaf))
        {
            throw "The package is missing theme '$($theme.id)'."
        }
        $themeHash = (Get-FileHash -LiteralPath $themePath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($themeHash -ne $theme.sha256)
        {
            throw "The packaged theme '$($theme.id)' does not match its manifest SHA-256."
        }

        $licensePath = Join-Path (Split-Path $themeManifestPath) ([string]$theme.licenseFile).Replace('/', '\')
        if (-not (Test-Path -LiteralPath $licensePath -PathType Leaf))
        {
            throw "The package is missing the license for theme '$($theme.id)'."
        }
    }

    $fontManifest = Get-Content -LiteralPath $fontManifestPath -Raw | ConvertFrom-Json
    foreach ($font in $fontManifest.fonts)
    {
        $fontPath = Join-Path $Path ([string]$font.packageFile).Replace('/', '\')
        $licensePath = Join-Path $Path ([string]$font.packageLicenseFile).Replace('/', '\')
        if (-not (Test-Path -LiteralPath $fontPath -PathType Leaf))
        {
            throw "The package is missing bundled font '$($font.id)'."
        }
        $fontHash = (Get-FileHash -LiteralPath $fontPath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($fontHash -ne $font.sha256)
        {
            throw "The packaged font '$($font.id)' does not match its manifest SHA-256."
        }
        if (-not (Test-Path -LiteralPath $licensePath -PathType Leaf))
        {
            throw "The package is missing the license for font '$($font.id)'."
        }
    }
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourceManifest = Join-Path $repositoryRoot 'src\cascadia\CascadiaPackage\Package-winTerm.appxmanifest'
$packageRoot = Join-Path $repositoryRoot 'src\cascadia\CascadiaPackage\AppPackages'
$temporaryDirectory = $null

try
{
    Assert-WinTermManifest -Path $sourceManifest -Publisher $ExpectedPublisher

    if (-not $SkipBuild)
    {
        & (Join-Path $PSScriptRoot 'build.ps1') -Configuration Release -Platform $Platform -GeneratePackage
        if ($LASTEXITCODE -ne 0)
        {
            throw "Release package build failed with exit code $LASTEXITCODE."
        }
    }

    if (-not (Test-Path -LiteralPath $packageRoot))
    {
        throw "Package output directory '$packageRoot' does not exist. Run without -SkipBuild first."
    }

    $artifact = Get-ChildItem -LiteralPath $packageRoot -Recurse -File -Filter '*.msix' |
        Where-Object { $_.FullName -match [regex]::Escape($Platform) } |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1

    if ($null -eq $artifact)
    {
        throw "No $Platform MSIX artifact was found under '$packageRoot'."
    }

    $makeAppx = Get-Command makeappx.exe -ErrorAction SilentlyContinue
    if ($null -eq $makeAppx)
    {
        throw 'makeappx.exe was not found. Install Windows SDK 10.0.22621.0 and run from a Visual Studio development environment.'
    }

    $temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("winterm-msix-{0}" -f [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null
    & $makeAppx.Source unpack /p $artifact.FullName /d $temporaryDirectory /o
    if ($LASTEXITCODE -ne 0)
    {
        throw "makeappx.exe failed to unpack '$($artifact.FullName)' with exit code $LASTEXITCODE."
    }

    Assert-WinTermManifest -Path (Join-Path $temporaryDirectory 'AppxManifest.xml') -Publisher $ExpectedPublisher
    Assert-AppearancePayload -Path $temporaryDirectory -RepositoryRoot $repositoryRoot
    & (Join-Path $PSScriptRoot 'package-shell-assets.ps1') -PackageRoot $temporaryDirectory
    if (-not $?)
    {
        throw 'Shell asset package validation failed.'
    }

    $signature = Get-AuthenticodeSignature -LiteralPath $artifact.FullName
    Write-Host "Package: $($artifact.FullName)"
    Write-Host "Architecture: $Platform"
    Write-Host "Signing status: $($signature.Status)"

    if ($signature.Status -ne 'Valid')
    {
        Write-Warning 'The package is not signed with a trusted certificate. Create a development certificate whose subject matches CN=winTerm Development, keep it out of Git, and rebuild before installation.'
    }

    $developerInstaller = Get-ChildItem -LiteralPath $artifact.Directory.FullName -File -Filter 'Add-AppDevPackage.ps1' -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $developerInstaller)
    {
        Write-Host "Install after reviewing the development certificate: & '$($developerInstaller.FullName)'"
    }
    else
    {
        Write-Host "Install a trusted signed package with: Add-AppxPackage -Path '$($artifact.FullName)'"
    }
}
catch
{
    Write-Error "winTerm packaging failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    if ($null -ne $temporaryDirectory -and (Test-Path -LiteralPath $temporaryDirectory))
    {
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
}
