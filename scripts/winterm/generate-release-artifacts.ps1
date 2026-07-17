# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^0\.6\.0-beta\.[0-9]+$')]
    [string]$Version,

    [Parameter(Mandatory)]
    [string]$ArtifactDirectory,

    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9a-fA-F]{7,64}$')]
    [string]$CommitSha,

    [Parameter()]
    [ValidateSet('x64', 'ARM64')]
    [string[]]$RequiredArchitecture = @('x64')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-ReleasePackage
{
    param(
        [Parameter(Mandatory)]
        [string]$Architecture,

        [Parameter(Mandatory)]
        [string]$Directory
    )

    $package = Get-ChildItem -LiteralPath $Directory -Recurse -File -Filter '*.msix' |
        Where-Object { $_.Name -match [regex]::Escape($Architecture) } |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if ($null -eq $package)
    {
        throw "No $Architecture MSIX package was found under '$Directory'."
    }
    return $package
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$inputDirectory = (Resolve-Path -LiteralPath $ArtifactDirectory).Path
$outputDirectory = Join-Path $inputDirectory "winTerm-$Version-release"
if (Test-Path -LiteralPath $outputDirectory)
{
    throw "Release output directory already exists: $outputDirectory"
}
New-Item -ItemType Directory -Path $outputDirectory | Out-Null

try
{
    $releaseFiles = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
    foreach ($architecture in $RequiredArchitecture)
    {
        $sourcePackage = Get-ReleasePackage -Architecture $architecture -Directory $inputDirectory
        $targetPackage = Join-Path $outputDirectory "winTerm-$Version-$($architecture.ToLowerInvariant()).msix"
        Copy-Item -LiteralPath $sourcePackage.FullName -Destination $targetPackage -ErrorAction Stop
        [void]$releaseFiles.Add((Get-Item -LiteralPath $targetPackage))
    }

    foreach ($requiredDocument in @('LICENSE', 'NOTICE.md', 'THIRD_PARTY_NOTICES.md'))
    {
        $source = Join-Path $repositoryRoot $requiredDocument
        Copy-Item -LiteralPath $source -Destination (Join-Path $outputDirectory $requiredDocument) -ErrorAction Stop
        [void]$releaseFiles.Add((Get-Item -LiteralPath (Join-Path $outputDirectory $requiredDocument)))
    }
    $releaseNotes = Join-Path $repositoryRoot "docs\releases\$Version.md"
    if (-not (Test-Path -LiteralPath $releaseNotes -PathType Leaf))
    {
        throw "Release notes are missing: $releaseNotes"
    }
    Copy-Item -LiteralPath $releaseNotes -Destination (Join-Path $outputDirectory 'release-notes.md') -ErrorAction Stop
    [void]$releaseFiles.Add((Get-Item -LiteralPath (Join-Path $outputDirectory 'release-notes.md')))

    $sbom = [ordered]@{
        SPDXID = 'SPDXRef-DOCUMENT'
        spdxVersion = 'SPDX-2.3'
        name = "winTerm-$Version"
        documentNamespace = "https://github.com/HelloThisWorld/winTerm/releases/$Version/$CommitSha"
        creationInfo = [ordered]@{
            created = [DateTime]::UtcNow.ToString('o')
            creators = @('Tool: winTerm release-artifact generator')
        }
        packages = @(
            [ordered]@{
                SPDXID = 'SPDXRef-winTerm'
                name = 'winTerm'
                versionInfo = $Version
                downloadLocation = 'NOASSERTION'
                licenseConcluded = 'MIT'
                licenseDeclared = 'MIT'
                supplier = 'NOASSERTION'
                externalRefs = @([ordered]@{ referenceCategory = 'PACKAGE-MANAGER'; referenceType = 'purl'; referenceLocator = 'pkg:github/HelloThisWorld/winTerm' })
            }
        )
        relationships = @()
    }
    $sbomPath = Join-Path $outputDirectory 'SBOM.spdx.json'
    $sbom | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $sbomPath -Encoding utf8NoBOM
    [void]$releaseFiles.Add((Get-Item -LiteralPath $sbomPath))

    $cycloneDx = [ordered]@{
        bomFormat = 'CycloneDX'
        specVersion = '1.5'
        serialNumber = "urn:uuid:$([guid]::NewGuid())"
        version = 1
        metadata = [ordered]@{
            timestamp = [DateTime]::UtcNow.ToString('o')
            component = [ordered]@{ type = 'application'; name = 'winTerm'; version = $Version }
        }
        components = @()
    }
    $cycloneDxPath = Join-Path $outputDirectory 'SBOM.cyclonedx.json'
    $cycloneDx | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $cycloneDxPath -Encoding utf8NoBOM
    [void]$releaseFiles.Add((Get-Item -LiteralPath $cycloneDxPath))

    $hashLines = foreach ($file in $releaseFiles | Sort-Object Name)
    {
        "$((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant())  $($file.Name)"
    }
    $hashLines | Set-Content -LiteralPath (Join-Path $outputDirectory 'SHA256SUMS.txt') -Encoding ascii

    $metadata = [ordered]@{
        schemaVersion = 1
        version = $Version
        packageVersion = '0.6.0.0'
        channel = 'beta'
        commitSha = $CommitSha.ToLowerInvariant()
        generatedAt = [DateTime]::UtcNow.ToString('o')
        signing = 'unsigned-development-package'
        artifacts = @($releaseFiles | Sort-Object Name | ForEach-Object {
            [ordered]@{
                fileName = $_.Name
                sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
                size = $_.Length
            }
        })
    }
    $metadata | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputDirectory 'release-metadata.json') -Encoding utf8NoBOM
    Write-Host "Prepared release artifacts in $outputDirectory" -ForegroundColor Green
}
catch
{
    Write-Error "Release artifact generation failed: $($_.Exception.Message)"
    exit 1
}
