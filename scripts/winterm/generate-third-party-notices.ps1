# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [switch]$Check
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Read-JsonFile
{
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf))
    {
        throw "Required manifest does not exist: $Path"
    }
    try
    {
        return [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8) | ConvertFrom-Json -ErrorAction Stop
    }
    catch
    {
        throw "Manifest parsing failed for '$Path': $($_.Exception.Message)"
    }
}

function Resolve-LocalPath
{
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath,

        [Parameter(Mandatory)]
        [string]$BaseDirectory,

        [Parameter(Mandatory)]
        [string]$RepositoryRoot,

        [Parameter(Mandatory)]
        [string]$Context
    )

    if ([string]::IsNullOrWhiteSpace($RelativePath) -or [System.IO.Path]::IsPathRooted($RelativePath))
    {
        throw "$Context must be a non-empty relative path."
    }
    $root = [System.IO.Path]::GetFullPath($RepositoryRoot).TrimEnd([char[]]@('\', '/'))
    $path = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($BaseDirectory, $RelativePath))
    $prefix = $root + [System.IO.Path]::DirectorySeparatorChar
    if (-not $path.Equals($root, [System.StringComparison]::OrdinalIgnoreCase) -and
        -not $path.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase))
    {
        throw "$Context escapes the repository root."
    }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf))
    {
        throw "$Context does not exist: $path"
    }
    return $path
}

function Get-RepositoryRelativePath
{
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$RepositoryRoot
    )

    $root = [System.IO.Path]::GetFullPath($RepositoryRoot).TrimEnd([char[]]@('\', '/'))
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $prefix = $root + [System.IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase))
    {
        throw "Path is outside the repository: $Path"
    }
    return $fullPath.Substring($prefix.Length).Replace('\', '/')
}

function Assert-ManifestHeader
{
    param(
        [Parameter(Mandatory)]
        [object]$Manifest,

        [Parameter(Mandatory)]
        [string]$ExpectedPathBase,

        [Parameter(Mandatory)]
        [string]$CollectionName,

        [Parameter(Mandatory)]
        [string]$Context
    )

    foreach ($propertyName in @('schemaVersion', 'pathBase', $CollectionName))
    {
        if ($null -eq $Manifest.PSObject.Properties[$propertyName])
        {
            throw "$Context is missing required property '$propertyName'."
        }
    }
    if ($Manifest.schemaVersion -ne 1)
    {
        throw "$Context uses an unsupported schemaVersion."
    }
    if ($Manifest.pathBase -cne $ExpectedPathBase)
    {
        throw "$Context pathBase must be '$ExpectedPathBase'."
    }
    if ($Manifest.PSObject.Properties[$CollectionName].Value -isnot [System.Array])
    {
        throw "$Context property '$CollectionName' must be a JSON array."
    }
}

function Add-InventorySection
{
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [AllowEmptyString()]
        [System.Collections.Generic.List[string]]$Lines,

        [Parameter(Mandatory)]
        [string]$Heading,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [object[]]$Items
    )

    [void]$Lines.Add("## $Heading")
    [void]$Lines.Add('')
    foreach ($item in $Items | Sort-Object Id)
    {
        [void]$Lines.Add("### $($item.Name)")
        [void]$Lines.Add('')
        [void]$Lines.Add("- ID: ``$($item.Id)``")
        [void]$Lines.Add("- Author: $($item.Author)")
        [void]$Lines.Add("- License: $($item.License)")
        [void]$Lines.Add("- Source project: ``$($item.SourceProject)``")
        [void]$Lines.Add("- Source revision: ``$($item.SourceRevision)``")
        [void]$Lines.Add("- Source file: ``$($item.SourceFile)``")
        [void]$Lines.Add("- Bundled asset: ``$($item.AssetFile)``")
        [void]$Lines.Add("- License file: ``$($item.LicenseFile)``")
        [void]$Lines.Add("- SHA-256: ``$($item.Sha256)``")
        if (-not [string]::IsNullOrWhiteSpace($item.Version))
        {
            [void]$Lines.Add("- Version: $($item.Version)")
        }
        [void]$Lines.Add('')
    }
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$themeManifestPath = Join-Path $repositoryRoot 'assets\winterm\themes\manifest.json'
$fontManifestPath = Join-Path $repositoryRoot 'assets\winterm\fonts\manifest.json'
$outputPath = Join-Path $repositoryRoot 'THIRD_PARTY_NOTICES.md'

try
{
    $themeManifest = Read-JsonFile -Path $themeManifestPath
    $fontManifest = Read-JsonFile -Path $fontManifestPath
    Assert-ManifestHeader -Manifest $themeManifest -ExpectedPathBase 'manifest' -CollectionName 'themes' -Context 'Theme manifest'
    Assert-ManifestHeader -Manifest $fontManifest -ExpectedPathBase 'repository' -CollectionName 'fonts' -Context 'Font manifest'

    $licensePaths = @{}
    $themes = @()
    foreach ($entry in @($themeManifest.themes))
    {
        $assetPath = Resolve-LocalPath -RelativePath ([string]$entry.file) -BaseDirectory (Split-Path -Parent $themeManifestPath) -RepositoryRoot $repositoryRoot -Context "Theme '$($entry.id)' file"
        $licensePath = Resolve-LocalPath -RelativePath ([string]$entry.licenseFile) -BaseDirectory (Split-Path -Parent $themeManifestPath) -RepositoryRoot $repositoryRoot -Context "Theme '$($entry.id)' licenseFile"
        $licensePaths[(Get-RepositoryRelativePath -Path $licensePath -RepositoryRoot $repositoryRoot)] = $licensePath
        $themes += [pscustomobject]@{
            Id = [string]$entry.id
            Name = [string]$entry.name
            Author = [string]$entry.author
            License = [string]$entry.license
            SourceProject = [string]$entry.sourceProject
            SourceRevision = [string]$entry.sourceRevision
            SourceFile = [string]$entry.sourceFile
            AssetFile = Get-RepositoryRelativePath -Path $assetPath -RepositoryRoot $repositoryRoot
            LicenseFile = Get-RepositoryRelativePath -Path $licensePath -RepositoryRoot $repositoryRoot
            Sha256 = [string]$entry.sha256
            Version = ''
        }
    }

    $fonts = @()
    foreach ($entry in @($fontManifest.fonts))
    {
        $assetPath = Resolve-LocalPath -RelativePath ([string]$entry.file) -BaseDirectory $repositoryRoot -RepositoryRoot $repositoryRoot -Context "Font '$($entry.id)' file"
        $licensePath = Resolve-LocalPath -RelativePath ([string]$entry.licenseFile) -BaseDirectory $repositoryRoot -RepositoryRoot $repositoryRoot -Context "Font '$($entry.id)' licenseFile"
        $licensePaths[(Get-RepositoryRelativePath -Path $licensePath -RepositoryRoot $repositoryRoot)] = $licensePath
        $fonts += [pscustomobject]@{
            Id = [string]$entry.id
            Name = [string]$entry.name
            Author = [string]$entry.author
            License = [string]$entry.license
            SourceProject = [string]$entry.sourceProject
            SourceRevision = [string]$entry.sourceRevision
            SourceFile = [string]$entry.sourceFile
            AssetFile = Get-RepositoryRelativePath -Path $assetPath -RepositoryRoot $repositoryRoot
            LicenseFile = Get-RepositoryRelativePath -Path $licensePath -RepositoryRoot $repositoryRoot
            Sha256 = [string]$entry.sha256
            Version = [string]$entry.version
        }
    }

    $lines = [System.Collections.Generic.List[string]]::new()
    [void]$lines.Add('# winTerm third-party notices')
    [void]$lines.Add('')
    [void]$lines.Add('This file is generated from the local appearance asset manifests and license files.')
    [void]$lines.Add('Do not edit it manually. Run `scripts/winterm/generate-third-party-notices.ps1` after an approved asset change.')
    [void]$lines.Add('')
    [void]$lines.Add('The Microsoft Terminal source baseline and its inherited dependencies remain covered by `NOTICE.md`. This file records the appearance assets added to or explicitly pinned by winTerm v0.2.')
    [void]$lines.Add('')
    Add-InventorySection -Lines $lines -Heading 'Themes' -Items $themes
    Add-InventorySection -Lines $lines -Heading 'Fonts' -Items $fonts
    [void]$lines.Add('## License texts')
    [void]$lines.Add('')
    foreach ($relativeLicensePath in @($licensePaths.Keys) | Sort-Object)
    {
        [void]$lines.Add("### ``$relativeLicensePath``")
        [void]$lines.Add('')
        [void]$lines.Add('```text')
        $licenseText = [System.IO.File]::ReadAllText([string]$licensePaths[$relativeLicensePath], [System.Text.Encoding]::UTF8)
        $normalizedLicense = ($licenseText -replace "`r`n?", "`n").TrimEnd()
        foreach ($licenseLine in $normalizedLicense -split "`n", -1)
        {
            [void]$lines.Add($licenseLine)
        }
        [void]$lines.Add('```')
        [void]$lines.Add('')
    }
    $expected = ([string]::Join("`n", $lines) + "`n")

    if ($Check)
    {
        if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf))
        {
            throw "Generated notice file does not exist: $outputPath"
        }
        $actual = [System.IO.File]::ReadAllText($outputPath, [System.Text.Encoding]::UTF8)
        if ($actual -cne $expected)
        {
            throw 'THIRD_PARTY_NOTICES.md is stale. Run scripts/winterm/generate-third-party-notices.ps1 and review the result.'
        }
        Write-Host 'winTerm third-party notices are current.' -ForegroundColor Green
    }
    else
    {
        [System.IO.File]::WriteAllText($outputPath, $expected, [System.Text.UTF8Encoding]::new($false))
        Write-Host "Generated $outputPath" -ForegroundColor Green
    }
}
catch
{
    Write-Error "winTerm third-party notice generation failed: $($_.Exception.Message)"
    exit 1
}
