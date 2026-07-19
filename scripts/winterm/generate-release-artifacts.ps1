# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('1.0.2')]
    [string]$Version,

    [Parameter(Mandatory)]
    [string]$X64PackagePath,

    [Parameter()]
    [string]$Arm64PackagePath,

    [Parameter()]
    [string]$BundlePath,

    [Parameter()]
    [string]$CertificatePath,

    [Parameter()]
    [string]$InstallationInstructionsPath,

    [Parameter(Mandatory)]
    [string]$SymbolsDirectory,

    [Parameter(Mandatory)]
    [string]$OutputDirectory,

    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string]$CommitSha,

    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string]$UpstreamRevision,

    [Parameter(Mandatory)]
    [ValidatePattern('^\d{4}-\d{2}-\d{2}T')]
    [string]$BuildTimestamp,

    [Parameter()]
    [ValidatePattern('^[0-9]+$')]
    [string]$WorkflowRunId = '0',

    [Parameter()]
    [ValidateSet('valid-production-signature', 'self-signed', 'unsigned-draft')]
    [string]$SigningStatus = 'unsigned-draft'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Copy-ReleaseFile
{
    param(
        [Parameter(Mandatory)]
        [string]$Source,

        [Parameter(Mandatory)]
        [string]$TargetName
    )

    $sourceItem = Get-Item -LiteralPath (Resolve-Path -LiteralPath $Source).Path
    if (($sourceItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)
    {
        throw "Release input '$($sourceItem.Name)' is a reparse point."
    }
    if ($sourceItem.Length -le 0)
    {
        throw "Release input '$($sourceItem.Name)' is empty."
    }

    $target = Join-Path $script:outputRoot $TargetName
    Copy-Item -LiteralPath $sourceItem.FullName -Destination $target -ErrorAction Stop
    return Get-Item -LiteralPath $target
}

function New-SbomComponent
{
    param(
        [Parameter(Mandatory)]
        [string]$Name,

        [Parameter(Mandatory)]
        [string]$VersionInfo,

        [Parameter(Mandatory)]
        [string]$Type,

        [Parameter()]
        [string]$License = 'NOASSERTION',

        [Parameter()]
        [string]$Revision
    )

    return [ordered]@{
        name = $Name
        version = $VersionInfo
        type = $Type
        license = $License
        revision = $Revision
    }
}

function Write-JsonWithoutBom
{
    param(
        [Parameter(Mandatory)]
        [object]$Value,

        [Parameter(Mandatory)]
        [string]$Path
    )

    $json = $Value | ConvertTo-Json -Depth 20
    [IO.File]::WriteAllText($Path, $json + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$outputRoot = if ([IO.Path]::IsPathRooted($OutputDirectory))
{
    [IO.Path]::GetFullPath($OutputDirectory)
}
else
{
    [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
}
$symbolsStaging = $null

try
{
    $timestamp = [DateTimeOffset]::Parse(
        $BuildTimestamp,
        [Globalization.CultureInfo]::InvariantCulture,
        [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    $commit = $CommitSha.ToLowerInvariant()
    $upstream = $UpstreamRevision.ToLowerInvariant()

    if (Test-Path -LiteralPath $outputRoot)
    {
        throw "Release output directory already exists: $outputRoot"
    }
    New-Item -ItemType Directory -Path $outputRoot | Out-Null

    $releaseFiles = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
    [void]$releaseFiles.Add((Copy-ReleaseFile -Source $X64PackagePath -TargetName "winTerm-$Version-x64.msix"))

    if ($SigningStatus -eq 'self-signed')
    {
        if ([string]::IsNullOrWhiteSpace($CertificatePath) -or
            [string]::IsNullOrWhiteSpace($InstallationInstructionsPath))
        {
            throw 'Self-signed releases require the public certificate and installation instructions.'
        }
        [void]$releaseFiles.Add((Copy-ReleaseFile -Source $CertificatePath -TargetName "winTerm-$Version.cer"))
        [void]$releaseFiles.Add((Copy-ReleaseFile -Source $InstallationInstructionsPath -TargetName 'INSTALL.txt'))
    }
    elseif (-not [string]::IsNullOrWhiteSpace($CertificatePath) -or
        -not [string]::IsNullOrWhiteSpace($InstallationInstructionsPath))
    {
        throw 'Certificate and installation instructions may be supplied only for a self-signed release.'
    }

    $hasArm64 = -not [string]::IsNullOrWhiteSpace($Arm64PackagePath)
    if ($hasArm64)
    {
        if ([string]::IsNullOrWhiteSpace($BundlePath))
        {
            throw 'An MSIX bundle is required when an ARM64 package is included.'
        }
        [void]$releaseFiles.Add((Copy-ReleaseFile -Source $Arm64PackagePath -TargetName "winTerm-$Version-arm64.msix"))
        [void]$releaseFiles.Add((Copy-ReleaseFile -Source $BundlePath -TargetName "winTerm-$Version.msixbundle"))
    }
    elseif (-not [string]::IsNullOrWhiteSpace($BundlePath))
    {
        throw 'A bundle must not be included without a validated ARM64 package.'
    }

    $notices = Copy-ReleaseFile -Source (Join-Path $repositoryRoot 'THIRD_PARTY_NOTICES.md') -TargetName 'THIRD_PARTY_NOTICES.md'
    [void]$releaseFiles.Add($notices)
    $releaseNotes = Copy-ReleaseFile -Source (Join-Path $repositoryRoot 'docs\releases\1.0.2.md') -TargetName "winTerm-$Version-release-notes.md"
    [void]$releaseFiles.Add($releaseNotes)

    $symbolsRoot = (Resolve-Path -LiteralPath $SymbolsDirectory).Path
    $pdbFiles = @(Get-ChildItem -LiteralPath $symbolsRoot -Recurse -File -Filter '*.pdb')
    if ($pdbFiles.Count -eq 0)
    {
        throw "No PDB files were found under '$symbolsRoot'."
    }
    $symbolsStaging = Join-Path ([IO.Path]::GetTempPath()) ("winterm-symbols-{0}" -f [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $symbolsStaging | Out-Null
    foreach ($pdb in $pdbFiles)
    {
        $relative = $pdb.FullName.Substring($symbolsRoot.Length).TrimStart('\', '/')
        $target = Join-Path $symbolsStaging $relative
        $targetDirectory = Split-Path -Parent $target
        if (-not (Test-Path -LiteralPath $targetDirectory))
        {
            New-Item -ItemType Directory -Path $targetDirectory | Out-Null
        }
        Copy-Item -LiteralPath $pdb.FullName -Destination $target
    }
    $symbolsPath = Join-Path $outputRoot "winTerm-$Version-symbols.zip"
    Compress-Archive -Path (Join-Path $symbolsStaging '*') -DestinationPath $symbolsPath -CompressionLevel Optimal
    [void]$releaseFiles.Add((Get-Item -LiteralPath $symbolsPath))

    $components = [System.Collections.Generic.List[object]]::new()
    [void]$components.Add((New-SbomComponent -Name 'winTerm' -VersionInfo $Version -Type 'application' -License 'MIT' -Revision $commit))
    [void]$components.Add((New-SbomComponent -Name 'Microsoft Terminal upstream source' -VersionInfo 'release-1.25' -Type 'framework' -License 'MIT' -Revision $upstream))
    [void]$components.Add((New-SbomComponent -Name 'winTerm PowerShell Module' -VersionInfo '1.0.2' -Type 'library' -License 'MIT' -Revision $commit))
    [void]$components.Add((New-SbomComponent -Name 'winterm-shim' -VersionInfo '1.0.2' -Type 'application' -License 'MIT' -Revision $commit))
    [void]$components.Add((New-SbomComponent -Name 'winTerm MSIX installer' -VersionInfo '1.0.2.0' -Type 'installer' -License 'MIT' -Revision $commit))

    $themeManifest = Get-Content -LiteralPath (Join-Path $repositoryRoot 'assets\winterm\themes\manifest.json') -Raw | ConvertFrom-Json
    foreach ($theme in $themeManifest.themes)
    {
        [void]$components.Add((New-SbomComponent -Name "Theme: $($theme.name)" -VersionInfo ([string]$theme.sourceRevision) -Type 'data' -License ([string]$theme.license) -Revision ([string]$theme.sourceRevision)))
    }

    $fontManifest = Get-Content -LiteralPath (Join-Path $repositoryRoot 'assets\winterm\fonts\manifest.json') -Raw | ConvertFrom-Json
    foreach ($font in $fontManifest.fonts)
    {
        [void]$components.Add((New-SbomComponent -Name "Font: $($font.fullName)" -VersionInfo ([string]$font.version) -Type 'file' -License ([string]$font.license) -Revision ([string]$font.sourceRevision)))
    }

    $vcpkg = Get-Content -LiteralPath (Join-Path $repositoryRoot 'vcpkg.json') -Raw | ConvertFrom-Json
    foreach ($dependency in $vcpkg.overrides)
    {
        [void]$components.Add((New-SbomComponent -Name ([string]$dependency.name) -VersionInfo ([string]$dependency.version) -Type 'library'))
    }

    [xml]$nuget = Get-Content -LiteralPath (Join-Path $repositoryRoot 'dep\nuget\packages.config') -Raw
    foreach ($package in $nuget.packages.package)
    {
        if ($package.GetAttribute('developmentDependency') -ne 'true')
        {
            [void]$components.Add((New-SbomComponent -Name ([string]$package.id) -VersionInfo ([string]$package.version) -Type 'library'))
        }
    }

    $spdxPackages = @($components | ForEach-Object {
        $safeId = ($_.name -replace '[^A-Za-z0-9.-]', '-').Trim('-')
        [ordered]@{
            SPDXID = "SPDXRef-$safeId"
            name = $_.name
            versionInfo = $_.version
            downloadLocation = 'NOASSERTION'
            filesAnalyzed = $false
            licenseConcluded = $_.license
            licenseDeclared = $_.license
            supplier = 'NOASSERTION'
            sourceInfo = if ([string]::IsNullOrWhiteSpace($_.revision)) { 'NOASSERTION' } else { "Revision $($_.revision)" }
        }
    })
    $spdx = [ordered]@{
        SPDXID = 'SPDXRef-DOCUMENT'
        spdxVersion = 'SPDX-2.3'
        dataLicense = 'CC0-1.0'
        name = "winTerm-$Version"
        documentNamespace = "https://github.com/HelloThisWorld/winTerm/releases/tag/v$Version/sbom/$commit"
        creationInfo = [ordered]@{
            created = $timestamp
            creators = @('Tool: winTerm release-artifact generator')
        }
        documentDescribes = @('SPDXRef-winTerm')
        packages = $spdxPackages
        relationships = @()
    }
    $spdxPath = Join-Path $outputRoot 'SBOM.spdx.json'
    Write-JsonWithoutBom -Value $spdx -Path $spdxPath
    [void]$releaseFiles.Add((Get-Item -LiteralPath $spdxPath))

    $seed = [Text.Encoding]::UTF8.GetBytes("winTerm:${Version}:$commit")
    $hash = [Security.Cryptography.SHA256]::Create().ComputeHash($seed)
    $uuidBytes = [byte[]]$hash[0..15]
    $serial = [guid]::new($uuidBytes)
    $cycloneComponents = @($components | ForEach-Object {
        [ordered]@{
            type = if ($_.type -in @('application', 'framework', 'library', 'file')) { $_.type } else { 'application' }
            'bom-ref' = "component:$($_.name):$($_.version)"
            name = $_.name
            version = $_.version
            licenses = if ($_.license -eq 'NOASSERTION') { @() } else { @([ordered]@{ license = [ordered]@{ id = $_.license } }) }
            properties = if ([string]::IsNullOrWhiteSpace($_.revision)) { @() } else { @([ordered]@{ name = 'winterm:sourceRevision'; value = $_.revision }) }
        }
    })
    $cycloneDx = [ordered]@{
        bomFormat = 'CycloneDX'
        specVersion = '1.6'
        serialNumber = "urn:uuid:$serial"
        version = 1
        metadata = [ordered]@{
            timestamp = $timestamp
            component = [ordered]@{ type = 'application'; 'bom-ref' = "component:winTerm:$Version"; name = 'winTerm'; version = $Version }
            properties = @(
                [ordered]@{ name = 'winterm:commitSha'; value = $commit },
                [ordered]@{ name = 'winterm:microsoftTerminalUpstreamRevision'; value = $upstream }
            )
        }
        components = $cycloneComponents
    }
    $cycloneDxPath = Join-Path $outputRoot 'SBOM.cyclonedx.json'
    Write-JsonWithoutBom -Value $cycloneDx -Path $cycloneDxPath
    [void]$releaseFiles.Add((Get-Item -LiteralPath $cycloneDxPath))

    $metadata = [ordered]@{
        schemaVersion = 1
        version = $Version
        packageVersion = '1.0.2.0'
        channel = 'stable'
        tag = "v$Version"
        commitSha = $commit
        buildTimestamp = $timestamp
        architecture = @('x64') + $(if ($hasArm64) { @('arm64') } else { @() })
        microsoftTerminalUpstreamRevision = $upstream
        workflowRunId = $WorkflowRunId
        workspaceSchemaVersion = 2
        dockingModelVersion = 1
        shellProtocolVersion = 1
        themeSchemaVersion = 1
        updateManifestSchemaVersion = 1
        signing = $SigningStatus
        artifacts = @($releaseFiles | Sort-Object Name | ForEach-Object {
            [ordered]@{
                fileName = $_.Name
                sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                size = $_.Length
            }
        })
    }
    $metadataPath = Join-Path $outputRoot 'release-metadata.json'
    Write-JsonWithoutBom -Value $metadata -Path $metadataPath

    & (Join-Path $PSScriptRoot 'generate-checksums.ps1') -Directory $outputRoot
    if (-not $?)
    {
        throw 'Checksum generation failed.'
    }
    & (Join-Path $PSScriptRoot 'verify-checksums.ps1') -Directory $outputRoot
    if (-not $?)
    {
        throw 'Checksum verification failed.'
    }

    Write-Host "Prepared winTerm $Version release artifacts in $outputRoot" -ForegroundColor Green
}
catch
{
    Write-Error "Release artifact generation failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    if ($null -ne $symbolsStaging -and (Test-Path -LiteralPath $symbolsStaging))
    {
        Remove-Item -LiteralPath $symbolsStaging -Recurse -Force
    }
}
