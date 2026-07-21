# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [Parameter()]
    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [string]$OutputDirectory,

    [Parameter()]
    [string]$BuildOutputRoot,

    [Parameter()]
    [switch]$SkipBuild,

    [Parameter()]
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$unpackagedOutputRoot = $null
$unpackagedExtractRoot = $null

function Assert-File
{
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf))
    {
        throw "$Description was not found at '$Path'."
    }
}

function Copy-DirectoryContent
{
    param(
        [Parameter(Mandatory)]
        [string]$Source,

        [Parameter(Mandatory)]
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Container))
    {
        throw "Required directory was not found: $Source"
    }
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Get-ChildItem -LiteralPath $Source -Force | Copy-Item -Destination $Destination -Recurse -Force
}

function Copy-RuntimeFile
{
    param(
        [Parameter(Mandatory)]
        [string]$Source,

        [Parameter(Mandatory)]
        [string]$Destination
    )

    Assert-File -Path $Source -Description 'Required runtime file'
    $parent = Split-Path -Parent $Destination
    if (-not (Test-Path -LiteralPath $parent))
    {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$metadata = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Branding\version.json') -Raw | ConvertFrom-Json
$platformLabel = $Platform.ToLowerInvariant()
$stageRoot = if ([string]::IsNullOrWhiteSpace($OutputDirectory))
{
    Join-Path $repositoryRoot "artifacts\stage\winTerm-$platformLabel"
}
elseif ([IO.Path]::IsPathRooted($OutputDirectory))
{
    [IO.Path]::GetFullPath($OutputDirectory)
}
else
{
    [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
}

try
{
    & (Join-Path $PSScriptRoot 'verify-branding.ps1') -ExpectedPublisher 'CN=helloThisWorld'
    if (-not $?)
    {
        throw 'Branding verification failed.'
    }

    if (-not $SkipBuild)
    {
        & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform -GeneratePackage
        if (-not $?)
        {
            throw 'The unpackaged binary build failed.'
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($BuildOutputRoot))
    {
        Write-Warning 'BuildOutputRoot is retained for command compatibility; the supported unpackaged layout is generated from the unsigned MSIX build intermediate.'
    }

    $packageVersion = [string]$metadata.packageVersion
    $terminalPackage = Join-Path $repositoryRoot "src\cascadia\CascadiaPackage\AppPackages\CascadiaPackage_${packageVersion}_${Platform}_Test\CascadiaPackage_${packageVersion}_${Platform}.msix"
    $xamlPackage = Join-Path $repositoryRoot "packages\Microsoft.UI.Xaml.2.8.4\tools\AppX\$Platform\Release\Microsoft.UI.Xaml.2.8.appx"
    Assert-File -Path $terminalPackage -Description 'Unsigned winTerm MSIX build intermediate'
    Assert-File -Path $xamlPackage -Description 'Pinned Microsoft.UI.Xaml framework package'

    # Reuse the Microsoft Terminal unpackaged distribution generator. It strips
    # package-identity files and merges the app and WinUI resource indexes so
    # Windows.UI.Xaml can run without package registration. The MSIX remains a
    # build intermediate and is never copied to a release payload.
    $unpackagedOutputRoot = Join-Path ([IO.Path]::GetTempPath()) ("winterm-unpackaged-output-{0}" -f [guid]::NewGuid().ToString('N'))
    $unpackagedExtractRoot = Join-Path ([IO.Path]::GetTempPath()) ("winterm-unpackaged-extract-{0}" -f [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $unpackagedOutputRoot, $unpackagedExtractRoot | Out-Null
    & (Join-Path $repositoryRoot 'build\scripts\New-UnpackagedTerminalDistribution.ps1') `
        -TerminalAppX $terminalPackage `
        -XamlAppX $xamlPackage `
        -Destination $unpackagedOutputRoot
    if (-not $?)
    {
        throw 'The upstream unpackaged distribution generator failed.'
    }
    $unpackagedArchives = @(Get-ChildItem -LiteralPath $unpackagedOutputRoot -Filter '*.zip' -File)
    if ($unpackagedArchives.Count -ne 1)
    {
        throw "Expected one upstream unpackaged archive, found $($unpackagedArchives.Count)."
    }
    $unpackagedArchive = $unpackagedArchives[0]
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory($unpackagedArchive.FullName, $unpackagedExtractRoot)
    $unpackagedPayloads = @(Get-ChildItem -LiteralPath $unpackagedExtractRoot -Directory)
    if ($unpackagedPayloads.Count -ne 1)
    {
        throw "Expected one upstream unpackaged payload directory, found $($unpackagedPayloads.Count)."
    }
    $unpackagedPayload = $unpackagedPayloads[0]
    Assert-File -Path (Join-Path $unpackagedPayload.FullName 'WindowsTerminal.exe') -Description 'Unpackaged winTerm host executable'

    if (Test-Path -LiteralPath $stageRoot)
    {
        if (-not $Force)
        {
            throw "Stage output already exists: $stageRoot. Pass -Force to replace this exact directory."
        }
        $resolvedStage = [IO.Path]::GetFullPath($stageRoot).TrimEnd('\')
        if ($resolvedStage.Length -lt 12 -or $resolvedStage -eq [IO.Path]::GetPathRoot($resolvedStage))
        {
            throw "Refusing to remove unsafe stage path '$resolvedStage'."
        }
        Remove-Item -LiteralPath $resolvedStage -Recurse -Force
    }
    New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null

    Get-ChildItem -LiteralPath $unpackagedPayload.FullName -Force | Copy-Item -Destination $stageRoot -Recurse -Force
    # Windows filenames are case-insensitive, so winTerm.exe also satisfies the
    # lowercase winterm.exe command alias. Use an intermediate name to preserve
    # the product casing in Explorer and installed-app metadata.
    $launcherPath = Join-Path $stageRoot 'winterm.exe'
    $temporaryLauncherPath = Join-Path $stageRoot 'winTerm-launcher.tmp'
    Move-Item -LiteralPath $launcherPath -Destination $temporaryLauncherPath
    Move-Item -LiteralPath $temporaryLauncherPath -Destination (Join-Path $stageRoot 'winTerm.exe')
    Copy-RuntimeFile -Source (Join-Path $stageRoot 'Microsoft.UI.Xaml.dll') -Destination (Join-Path $stageRoot 'runtime\Microsoft.UI.Xaml.dll')

    Copy-DirectoryContent -Source (Join-Path $repositoryRoot 'shell\powershell') -Destination (Join-Path $stageRoot 'shell\powershell')
    Copy-DirectoryContent -Source (Join-Path $repositoryRoot 'shell\cmd') -Destination (Join-Path $stageRoot 'shell\cmd')
    Copy-RuntimeFile -Source (Join-Path $repositoryRoot 'shell\shared\version.json') -Destination (Join-Path $stageRoot 'shell\version.json')
    Copy-DirectoryContent -Source (Join-Path $stageRoot 'shell') -Destination (Join-Path $stageRoot 'ShellAssets')
    Copy-RuntimeFile -Source (Join-Path $stageRoot 'winterm-shim.exe') -Destination (Join-Path $stageRoot 'ShellAssets\winterm-shim.exe')

    Copy-DirectoryContent -Source (Join-Path $repositoryRoot 'assets\winterm\themes') -Destination (Join-Path $stageRoot 'assets\themes')
    Copy-DirectoryContent -Source (Join-Path $repositoryRoot 'assets\winterm\icons') -Destination (Join-Path $stageRoot 'assets\icons')
    foreach ($nonRuntimeIconFile in @('generate_icons.py', 'README.md'))
    {
        $nonRuntimeIconPath = Join-Path $stageRoot "assets\icons\$nonRuntimeIconFile"
        if (Test-Path -LiteralPath $nonRuntimeIconPath -PathType Leaf)
        {
            Remove-Item -LiteralPath $nonRuntimeIconPath -Force
        }
    }
    Copy-DirectoryContent -Source (Join-Path $repositoryRoot 'assets\winterm\licenses') -Destination (Join-Path $stageRoot 'assets\licenses')
    Copy-DirectoryContent -Source (Join-Path $repositoryRoot 'assets\winterm\themes') -Destination (Join-Path $stageRoot 'AppearanceAssets\themes')
    Copy-DirectoryContent -Source (Join-Path $repositoryRoot 'assets\winterm\licenses') -Destination (Join-Path $stageRoot 'AppearanceAssets\licenses')
    Copy-DirectoryContent -Source (Join-Path $repositoryRoot 'assets\winterm\samples') -Destination (Join-Path $stageRoot 'AppearanceAssets\samples')

    foreach ($fontsRoot in @('assets\fonts', 'AppearanceAssets\fonts\bundled'))
    {
        $destination = Join-Path $stageRoot $fontsRoot
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        Get-ChildItem -LiteralPath (Join-Path $repositoryRoot 'res\fonts') -File | Where-Object { $_.Extension -in @('.ttf', '.otf') } | Copy-Item -Destination $destination -Force
    }
    Copy-RuntimeFile -Source (Join-Path $repositoryRoot 'assets\winterm\fonts\manifest.json') -Destination (Join-Path $stageRoot 'assets\fonts\manifest.json')
    Copy-RuntimeFile -Source (Join-Path $repositoryRoot 'assets\winterm\fonts\manifest.json') -Destination (Join-Path $stageRoot 'AppearanceAssets\fonts\manifest.json')

    Copy-DirectoryContent -Source (Join-Path $repositoryRoot 'res\terminal\images-WinTerm') -Destination (Join-Path $stageRoot 'Images')
    Copy-DirectoryContent -Source (Join-Path $repositoryRoot 'src\cascadia\CascadiaPackage\ProfileIcons') -Destination (Join-Path $stageRoot 'ProfileIcons')
    Copy-DirectoryContent -Source (Join-Path $repositoryRoot 'src\cascadia\CascadiaPackage\ProfileGeneratorIcons') -Destination (Join-Path $stageRoot 'ProfileGeneratorIcons')
    Copy-RuntimeFile -Source (Join-Path $repositoryRoot 'src\cascadia\TerminalSettingsModel\defaults.json') -Destination (Join-Path $stageRoot 'defaults.json')

    $licensesRoot = Join-Path $stageRoot 'licenses'
    New-Item -ItemType Directory -Path $licensesRoot -Force | Out-Null
    foreach ($name in @('LICENSE', 'NOTICE.md', 'THIRD_PARTY_NOTICES.md'))
    {
        Copy-RuntimeFile -Source (Join-Path $repositoryRoot $name) -Destination (Join-Path $licensesRoot $name)
        if ($name -eq 'THIRD_PARTY_NOTICES.md')
        {
            Copy-RuntimeFile -Source (Join-Path $repositoryRoot $name) -Destination (Join-Path $stageRoot $name)
        }
    }

    $safeRepositoryRoot = $repositoryRoot.Replace('\', '/')
    $commit = (& git -c "safe.directory=$safeRepositoryRoot" -C $repositoryRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$')
    {
        throw 'Unable to resolve the source commit for version.json.'
    }
    $stageMetadata = [ordered]@{
        schemaVersion = 1
        application = 'winTerm'
        publisher = 'helloThisWorld'
        productId = 'HelloThisWorld.winTerm'
        applicationVersion = [string]$metadata.applicationVersion
        architecture = $platformLabel
        distribution = 'unpackaged-self-contained'
        sourceCommit = $commit
        microsoftTerminalUpstreamRevision = [string]$metadata.microsoftTerminalUpstreamRevision
    }
    [IO.File]::WriteAllText(
        (Join-Path $stageRoot 'version.json'),
        ($stageMetadata | ConvertTo-Json -Depth 8) + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))

    & (Join-Path $PSScriptRoot 'verify-release-layout.ps1') -Path $stageRoot -Type Stage -Platform $Platform
    if (-not $?)
    {
        throw 'Unpackaged stage verification failed.'
    }

    Write-Host "Prepared unpackaged winTerm stage: $stageRoot" -ForegroundColor Green
}
catch
{
    Write-Error "Unpackaged build failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    foreach ($temporaryPath in @($unpackagedOutputRoot, $unpackagedExtractRoot))
    {
        if ($null -ne $temporaryPath -and (Test-Path -LiteralPath $temporaryPath))
        {
            Remove-Item -LiteralPath $temporaryPath -Recurse -Force
        }
    }
}
