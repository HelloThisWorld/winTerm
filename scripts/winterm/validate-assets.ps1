# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-RequiredProperty
{
    param(
        [Parameter(Mandatory)]
        [object]$Object,

        [Parameter(Mandatory)]
        [string]$Name,

        [Parameter(Mandatory)]
        [string]$Context
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property)
    {
        throw "$Context is missing required property '$Name'."
    }
    return $property.Value
}

function Assert-ObjectSchema
{
    param(
        [Parameter(Mandatory)]
        [object]$Object,

        [Parameter(Mandatory)]
        [string[]]$Required,

        [Parameter(Mandatory)]
        [string[]]$Allowed,

        [Parameter(Mandatory)]
        [string]$Context
    )

    if ($null -eq $Object -or $Object -is [string] -or $Object -is [System.Array] -or $Object -is [System.ValueType])
    {
        throw "$Context must be a JSON object."
    }

    foreach ($name in $Required)
    {
        if ($null -eq $Object.PSObject.Properties[$name])
        {
            throw "$Context is missing required property '$name'."
        }
    }
    foreach ($property in $Object.PSObject.Properties)
    {
        if ($Allowed -notcontains $property.Name)
        {
            throw "$Context contains unsupported property '$($property.Name)'."
        }
    }
}

function Assert-NonEmptyString
{
    param(
        [Parameter(Mandatory)]
        [AllowNull()]
        [object]$Value,

        [Parameter(Mandatory)]
        [string]$Context
    )

    if ($Value -isnot [string] -or [string]::IsNullOrWhiteSpace([string]$Value))
    {
        throw "$Context must be a non-empty string."
    }
}

function Test-Integer
{
    param(
        [Parameter(Mandatory)]
        [AllowNull()]
        [object]$Value
    )

    return $Value -is [sbyte] -or
        $Value -is [byte] -or
        $Value -is [int16] -or
        $Value -is [uint16] -or
        $Value -is [int32] -or
        $Value -is [uint32] -or
        $Value -is [int64] -or
        $Value -is [uint64]
}

function Read-JsonFile
{
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf))
    {
        throw "Required JSON file does not exist: $Path"
    }
    $content = [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8)
    try
    {
        return $content | ConvertFrom-Json -ErrorAction Stop
    }
    catch
    {
        throw "JSON parsing failed for '$Path': $($_.Exception.Message)"
    }
}

function Resolve-ManifestPath
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

    Assert-NonEmptyString -Value $RelativePath -Context $Context
    if ([System.IO.Path]::IsPathRooted($RelativePath))
    {
        throw "$Context must be a relative path."
    }

    $root = [System.IO.Path]::GetFullPath($RepositoryRoot).TrimEnd([char[]]@('\', '/'))
    $candidate = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($BaseDirectory, $RelativePath))
    $rootPrefix = $root + [System.IO.Path]::DirectorySeparatorChar
    if (-not $candidate.Equals($root, [System.StringComparison]::OrdinalIgnoreCase) -and
        -not $candidate.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase))
    {
        throw "$Context escapes the repository root."
    }
    return $candidate
}

function Assert-Extension
{
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [AllowEmptyString()]
        [string[]]$Allowed,

        [Parameter(Mandatory)]
        [string]$Context
    )

    $extension = [System.IO.Path]::GetExtension($Path).ToLowerInvariant()
    if ($Allowed -notcontains $extension)
    {
        throw "$Context uses unsupported extension '$extension'."
    }
}

function Assert-FileAndHash
{
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$ExpectedHash,

        [Parameter(Mandatory)]
        [string]$Context
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf))
    {
        throw "$Context does not exist: $Path"
    }
    if ($ExpectedHash -notmatch '^[0-9a-f]{64}$')
    {
        throw "$Context has an invalid SHA-256 value; lowercase hexadecimal is required."
    }
    $actualHash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -cne $ExpectedHash)
    {
        throw "$Context SHA-256 mismatch. Expected $ExpectedHash but found $actualHash."
    }
}

function Assert-AssetId
{
    param(
        [Parameter(Mandatory)]
        [string]$Id,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.HashSet[string]]$Ids,

        [Parameter(Mandatory)]
        [string]$Context
    )

    if ($Id -cnotmatch '^[a-z0-9][a-z0-9._-]{2,127}$')
    {
        throw "$Context has invalid ID '$Id'."
    }
    if (-not $Ids.Add($Id))
    {
        throw "Duplicate asset ID: $Id"
    }
}

function Assert-CanonicalColor
{
    param(
        [Parameter(Mandatory)]
        [AllowNull()]
        [object]$Value,

        [Parameter(Mandatory)]
        [string]$Context
    )

    if ($Value -isnot [string] -or $Value -cnotmatch '^#[0-9A-F]{6}([0-9A-F]{2})?$')
    {
        throw "$Context must use canonical #RRGGBB or #AARRGGBB format."
    }
}

function Assert-ThemeDescriptor
{
    param(
        [Parameter(Mandatory)]
        [object]$Theme,

        [Parameter(Mandatory)]
        [object]$ManifestEntry,

        [Parameter(Mandatory)]
        [string]$Context
    )

    $topRequired = @('schemaVersion', 'id', 'name', 'displayName', 'variant', 'source', 'terminal', 'window')
    $topAllowed = $topRequired + @('originalFileName')
    Assert-ObjectSchema -Object $Theme -Required $topRequired -Allowed $topAllowed -Context $Context

    if ((Get-RequiredProperty -Object $Theme -Name 'schemaVersion' -Context $Context) -ne 1)
    {
        throw "$Context uses an unsupported schemaVersion."
    }
    $themeId = [string](Get-RequiredProperty -Object $Theme -Name 'id' -Context $Context)
    $manifestId = [string](Get-RequiredProperty -Object $ManifestEntry -Name 'id' -Context "$Context manifest entry")
    if ($themeId -cne $manifestId)
    {
        throw "$Context ID '$themeId' does not match manifest ID '$manifestId'."
    }
    foreach ($field in @('name', 'displayName'))
    {
        Assert-NonEmptyString -Value (Get-RequiredProperty -Object $Theme -Name $field -Context $Context) -Context "$Context.$field"
    }
    $variant = [string](Get-RequiredProperty -Object $Theme -Name 'variant' -Context $Context)
    if (@('dark', 'light', 'highContrast', 'adaptive') -cnotcontains $variant)
    {
        throw "$Context has unsupported variant '$variant'."
    }

    $source = Get-RequiredProperty -Object $Theme -Name 'source' -Context $Context
    $sourceRequired = @('type', 'project', 'author', 'homepage', 'license', 'revision', 'sourceFile')
    Assert-ObjectSchema -Object $source -Required $sourceRequired -Allowed $sourceRequired -Context "$Context.source"
    foreach ($field in @('type', 'project', 'author', 'license', 'revision', 'sourceFile'))
    {
        Assert-NonEmptyString -Value (Get-RequiredProperty -Object $source -Name $field -Context "$Context.source") -Context "$Context.source.$field"
    }
    $sourceType = [string](Get-RequiredProperty -Object $source -Name 'type' -Context "$Context.source")
    if (@('builtin', 'thirdParty') -cnotcontains $sourceType)
    {
        throw "$Context.source.type must be 'builtin' or 'thirdParty'."
    }
    $homepage = Get-RequiredProperty -Object $source -Name 'homepage' -Context "$Context.source"
    if ($null -ne $homepage -and ($homepage -isnot [string] -or [string]::IsNullOrWhiteSpace([string]$homepage)))
    {
        throw "$Context.source.homepage must be null or a non-empty string."
    }
    $sourcePairs = @(
        @('project', 'sourceProject'),
        @('author', 'author'),
        @('license', 'license'),
        @('revision', 'sourceRevision'),
        @('sourceFile', 'sourceFile')
    )
    foreach ($pair in $sourcePairs)
    {
        $descriptorValue = [string](Get-RequiredProperty -Object $source -Name $pair[0] -Context "$Context.source")
        $manifestValue = [string](Get-RequiredProperty -Object $ManifestEntry -Name $pair[1] -Context "$Context manifest entry")
        if ($descriptorValue -cne $manifestValue)
        {
            throw "$Context source metadata '$($pair[0])' does not match the manifest."
        }
    }

    $terminal = Get-RequiredProperty -Object $Theme -Name 'terminal' -Context $Context
    $terminalFields = @('foreground', 'background', 'cursorColor', 'cursorTextColor', 'selectionBackground', 'selectionForeground', 'ansi', 'bright')
    Assert-ObjectSchema -Object $terminal -Required $terminalFields -Allowed $terminalFields -Context "$Context.terminal"
    foreach ($field in @('foreground', 'background', 'cursorColor', 'cursorTextColor', 'selectionBackground', 'selectionForeground'))
    {
        Assert-CanonicalColor -Value (Get-RequiredProperty -Object $terminal -Name $field -Context "$Context.terminal") -Context "$Context.terminal.$field"
    }
    foreach ($paletteName in @('ansi', 'bright'))
    {
        $palette = @(Get-RequiredProperty -Object $terminal -Name $paletteName -Context "$Context.terminal")
        if ($palette.Count -ne 8)
        {
            throw "$Context.terminal.$paletteName must contain exactly eight colors."
        }
        for ($index = 0; $index -lt $palette.Count; $index++)
        {
            Assert-CanonicalColor -Value $palette[$index] -Context "$Context.terminal.$paletteName[$index]"
        }
    }

    $window = Get-RequiredProperty -Object $Theme -Name 'window' -Context $Context
    $windowFields = @('opacity', 'useAcrylic', 'tabBarBackground', 'inactiveTabBackground', 'paneBorderColor')
    Assert-ObjectSchema -Object $window -Required $windowFields -Allowed $windowFields -Context "$Context.window"
    $opacity = Get-RequiredProperty -Object $window -Name 'opacity' -Context "$Context.window"
    if ($opacity -is [bool] -or $opacity -isnot [System.ValueType])
    {
        throw "$Context.window.opacity must be numeric."
    }
    $opacityValue = [double]$opacity
    if ([double]::IsNaN($opacityValue) -or [double]::IsInfinity($opacityValue) -or $opacityValue -lt 0.0 -or $opacityValue -gt 1.0)
    {
        throw "$Context.window.opacity must be from 0.0 through 1.0."
    }
    if ((Get-RequiredProperty -Object $window -Name 'useAcrylic' -Context "$Context.window") -isnot [bool])
    {
        throw "$Context.window.useAcrylic must be a boolean."
    }
    foreach ($field in @('tabBarBackground', 'inactiveTabBackground', 'paneBorderColor'))
    {
        Assert-CanonicalColor -Value (Get-RequiredProperty -Object $window -Name $field -Context "$Context.window") -Context "$Context.window.$field"
    }
}

function Test-ThemeManifest
{
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$RepositoryRoot,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.HashSet[string]]$Ids
    )

    $manifest = Read-JsonFile -Path $Path
    $manifestFields = @('schemaVersion', 'pathBase', 'themes')
    Assert-ObjectSchema -Object $manifest -Required $manifestFields -Allowed $manifestFields -Context 'Theme manifest'
    if ((Get-RequiredProperty -Object $manifest -Name 'schemaVersion' -Context 'Theme manifest') -ne 1)
    {
        throw 'Theme manifest uses an unsupported schemaVersion.'
    }
    if ((Get-RequiredProperty -Object $manifest -Name 'pathBase' -Context 'Theme manifest') -cne 'manifest')
    {
        throw "Theme manifest pathBase must be 'manifest'."
    }
    $themeCollection = $manifest.PSObject.Properties['themes'].Value
    if ($themeCollection -isnot [System.Array])
    {
        throw 'Theme manifest themes property must be a JSON array.'
    }
    $entries = @($themeCollection)
    if ($entries.Count -eq 0)
    {
        throw 'Theme manifest must contain at least one theme.'
    }

    $entryFields = @('id', 'name', 'file', 'author', 'license', 'licenseFile', 'sourceProject', 'sourceRevision', 'sourceFile', 'sha256')
    $manifestDirectory = Split-Path -Parent $Path
    $manifestAssetPaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $themeRootPrefix = [System.IO.Path]::GetFullPath($manifestDirectory).TrimEnd([char[]]@('\', '/')) + [System.IO.Path]::DirectorySeparatorChar
    foreach ($entry in $entries)
    {
        Assert-ObjectSchema -Object $entry -Required $entryFields -Allowed $entryFields -Context 'Theme manifest entry'
        $id = [string](Get-RequiredProperty -Object $entry -Name 'id' -Context 'Theme manifest entry')
        Assert-AssetId -Id $id -Ids $Ids -Context 'Theme manifest entry'
        foreach ($field in $entryFields)
        {
            Assert-NonEmptyString -Value (Get-RequiredProperty -Object $entry -Name $field -Context "Theme '$id'") -Context "Theme '$id'.$field"
        }

        $file = [string](Get-RequiredProperty -Object $entry -Name 'file' -Context "Theme '$id'")
        $assetPath = Resolve-ManifestPath -RelativePath $file -BaseDirectory $manifestDirectory -RepositoryRoot $RepositoryRoot -Context "Theme '$id' file"
        if (-not $assetPath.StartsWith($themeRootPrefix, [System.StringComparison]::OrdinalIgnoreCase))
        {
            throw "Theme '$id' file must remain in the theme asset tree."
        }
        if (-not $manifestAssetPaths.Add([System.IO.Path]::GetFullPath($assetPath)))
        {
            throw "Theme '$id' reuses a file that is already registered in the manifest."
        }
        Assert-Extension -Path $assetPath -Allowed @('.json') -Context "Theme '$id' file"
        Assert-FileAndHash -Path $assetPath -ExpectedHash ([string](Get-RequiredProperty -Object $entry -Name 'sha256' -Context "Theme '$id'")) -Context "Theme '$id' file"

        $licensePath = Resolve-ManifestPath -RelativePath ([string](Get-RequiredProperty -Object $entry -Name 'licenseFile' -Context "Theme '$id'")) -BaseDirectory $manifestDirectory -RepositoryRoot $RepositoryRoot -Context "Theme '$id' licenseFile"
        Assert-Extension -Path $licensePath -Allowed @('', '.txt', '.md') -Context "Theme '$id' licenseFile"
        if (-not (Test-Path -LiteralPath $licensePath -PathType Leaf))
        {
            throw "Theme '$id' license file does not exist: $licensePath"
        }

        $theme = Read-JsonFile -Path $assetPath
        Assert-ThemeDescriptor -Theme $theme -ManifestEntry $entry -Context "Theme '$id'"
        if ($file -like 'builtin/*' -and ([string](Get-RequiredProperty -Object (Get-RequiredProperty -Object $theme -Name 'source' -Context "Theme '$id'") -Name 'type' -Context "Theme '$id'.source")) -cne 'builtin')
        {
            throw "Theme '$id' in the builtin directory must use source.type 'builtin'."
        }
        if ($file -like 'third-party/*' -and ([string](Get-RequiredProperty -Object (Get-RequiredProperty -Object $theme -Name 'source' -Context "Theme '$id'") -Name 'type' -Context "Theme '$id'.source")) -cne 'thirdParty')
        {
            throw "Theme '$id' in the third-party directory must use source.type 'thirdParty'."
        }
        Write-Host "PASS: Theme asset $id" -ForegroundColor Green
    }

    $themeRoot = Split-Path -Parent $Path
    foreach ($asset in Get-ChildItem -LiteralPath $themeRoot -Recurse -File)
    {
        $extension = $asset.Extension.ToLowerInvariant()
        if ($asset.FullName.Equals([System.IO.Path]::GetFullPath($Path), [System.StringComparison]::OrdinalIgnoreCase))
        {
            continue
        }
        if ($asset.Name -ne '.keep' -and $extension -ne '.json')
        {
            throw "Unsupported file in the theme asset tree: $($asset.FullName)"
        }
        if ($asset.Name -ne '.keep' -and -not $manifestAssetPaths.Contains([System.IO.Path]::GetFullPath($asset.FullName)))
        {
            throw "Theme asset is not registered in the manifest: $($asset.FullName)"
        }
    }
    return $entries.Count
}

function Test-FontManifest
{
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$RepositoryRoot,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.HashSet[string]]$Ids
    )

    $manifest = Read-JsonFile -Path $Path
    $manifestFields = @('schemaVersion', 'pathBase', 'fonts')
    Assert-ObjectSchema -Object $manifest -Required $manifestFields -Allowed $manifestFields -Context 'Font manifest'
    if ((Get-RequiredProperty -Object $manifest -Name 'schemaVersion' -Context 'Font manifest') -ne 1)
    {
        throw 'Font manifest uses an unsupported schemaVersion.'
    }
    if ((Get-RequiredProperty -Object $manifest -Name 'pathBase' -Context 'Font manifest') -cne 'repository')
    {
        throw "Font manifest pathBase must be 'repository'."
    }
    $fontCollection = $manifest.PSObject.Properties['fonts'].Value
    if ($fontCollection -isnot [System.Array])
    {
        throw 'Font manifest fonts property must be a JSON array.'
    }
    $entries = @($fontCollection)
    if ($entries.Count -eq 0)
    {
        throw 'Font manifest must contain at least one font.'
    }

    $entryFields = @('id', 'name', 'family', 'fullName', 'postScriptName', 'style', 'weightRange', 'monospaced', 'ligatures', 'file', 'packageFile', 'version', 'fontVersion', 'author', 'license', 'licenseFile', 'packageLicenseFile', 'sourceProject', 'sourceRevision', 'sourceFile', 'sha256')
    $manifestAssetPaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $fontRoot = [System.IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'res\fonts')).TrimEnd([char[]]@('\', '/'))
    $fontRootPrefix = $fontRoot + [System.IO.Path]::DirectorySeparatorChar
    foreach ($entry in $entries)
    {
        Assert-ObjectSchema -Object $entry -Required $entryFields -Allowed $entryFields -Context 'Font manifest entry'
        $id = [string](Get-RequiredProperty -Object $entry -Name 'id' -Context 'Font manifest entry')
        Assert-AssetId -Id $id -Ids $Ids -Context 'Font manifest entry'
        foreach ($field in @('name', 'family', 'fullName', 'postScriptName', 'style', 'file', 'packageFile', 'version', 'fontVersion', 'author', 'license', 'licenseFile', 'packageLicenseFile', 'sourceProject', 'sourceRevision', 'sourceFile', 'sha256'))
        {
            Assert-NonEmptyString -Value (Get-RequiredProperty -Object $entry -Name $field -Context "Font '$id'") -Context "Font '$id'.$field"
        }
        if (@('normal', 'italic', 'oblique') -cnotcontains [string](Get-RequiredProperty -Object $entry -Name 'style' -Context "Font '$id'"))
        {
            throw "Font '$id' has an unsupported style."
        }
        foreach ($field in @('monospaced', 'ligatures'))
        {
            if ((Get-RequiredProperty -Object $entry -Name $field -Context "Font '$id'") -isnot [bool])
            {
                throw "Font '$id'.$field must be a boolean."
            }
        }

        $weightRange = Get-RequiredProperty -Object $entry -Name 'weightRange' -Context "Font '$id'"
        Assert-ObjectSchema -Object $weightRange -Required @('min', 'max') -Allowed @('min', 'max') -Context "Font '$id'.weightRange"
        $minimumWeight = Get-RequiredProperty -Object $weightRange -Name 'min' -Context "Font '$id'.weightRange"
        $maximumWeight = Get-RequiredProperty -Object $weightRange -Name 'max' -Context "Font '$id'.weightRange"
        if (-not (Test-Integer -Value $minimumWeight) -or
            -not (Test-Integer -Value $maximumWeight) -or
            $minimumWeight -lt 1 -or
            $maximumWeight -gt 1000 -or
            $minimumWeight -gt $maximumWeight)
        {
            throw "Font '$id'.weightRange must contain integer min/max values from 1 through 1000."
        }

        $assetPath = Resolve-ManifestPath -RelativePath ([string](Get-RequiredProperty -Object $entry -Name 'file' -Context "Font '$id'")) -BaseDirectory $RepositoryRoot -RepositoryRoot $RepositoryRoot -Context "Font '$id' file"
        if (-not $assetPath.StartsWith($fontRootPrefix, [System.StringComparison]::OrdinalIgnoreCase))
        {
            throw "Font '$id' file must remain in the bundled font resource directory."
        }
        if (-not $manifestAssetPaths.Add([System.IO.Path]::GetFullPath($assetPath)))
        {
            throw "Font '$id' reuses a file that is already registered in the manifest."
        }
        Assert-Extension -Path $assetPath -Allowed @('.ttf', '.otf') -Context "Font '$id' file"
        Assert-FileAndHash -Path $assetPath -ExpectedHash ([string](Get-RequiredProperty -Object $entry -Name 'sha256' -Context "Font '$id'")) -Context "Font '$id' file"

        $expectedPackageFile = 'AppearanceAssets/fonts/bundled/' + [System.IO.Path]::GetFileName($assetPath)
        if ([string](Get-RequiredProperty -Object $entry -Name 'packageFile' -Context "Font '$id'") -cne $expectedPackageFile)
        {
            throw "Font '$id'.packageFile must match '$expectedPackageFile'."
        }

        $licensePath = Resolve-ManifestPath -RelativePath ([string](Get-RequiredProperty -Object $entry -Name 'licenseFile' -Context "Font '$id'")) -BaseDirectory $RepositoryRoot -RepositoryRoot $RepositoryRoot -Context "Font '$id' licenseFile"
        Assert-Extension -Path $licensePath -Allowed @('', '.txt', '.md') -Context "Font '$id' licenseFile"
        if (-not (Test-Path -LiteralPath $licensePath -PathType Leaf))
        {
            throw "Font '$id' license file does not exist: $licensePath"
        }
        $expectedPackageLicenseFile = 'AppearanceAssets/licenses/fonts/' + [System.IO.Path]::GetFileName($licensePath)
        if ([string](Get-RequiredProperty -Object $entry -Name 'packageLicenseFile' -Context "Font '$id'") -cne $expectedPackageLicenseFile)
        {
            throw "Font '$id'.packageLicenseFile must match '$expectedPackageLicenseFile'."
        }
        Write-Host "PASS: Font asset $id" -ForegroundColor Green
    }
    foreach ($asset in Get-ChildItem -LiteralPath $fontRoot -File)
    {
        if ($asset.Extension.ToLowerInvariant() -in @('.ttf', '.otf') -and -not $manifestAssetPaths.Contains([System.IO.Path]::GetFullPath($asset.FullName)))
        {
            throw "Bundled font asset is not registered in the manifest: $($asset.FullName)"
        }
    }
    return $entries.Count
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$themeManifestPath = Join-Path $repositoryRoot 'assets\winterm\themes\manifest.json'
$fontManifestPath = Join-Path $repositoryRoot 'assets\winterm\fonts\manifest.json'

try
{
    $ids = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $themeCount = Test-ThemeManifest -Path $themeManifestPath -RepositoryRoot $repositoryRoot -Ids $ids
    $fontCount = Test-FontManifest -Path $fontManifestPath -RepositoryRoot $repositoryRoot -Ids $ids
    Write-Host "winTerm appearance asset validation passed ($themeCount themes, $fontCount fonts)." -ForegroundColor Green
}
catch
{
    Write-Error "winTerm appearance asset validation failed: $($_.Exception.Message)"
    exit 1
}
