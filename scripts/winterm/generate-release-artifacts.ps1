# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$')]
    [string]$Version,

    [Parameter(Mandatory)]
    [string]$InstallerPath,

    [Parameter(Mandatory)]
    [string]$PortablePath,

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
    [ValidateSet('trusted-signed', 'unsigned')]
    [string]$SigningStatus = 'unsigned',

    [Parameter()]
    [string]$ReleaseNotesPath
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
    Copy-Item -LiteralPath $sourceItem.FullName -Destination $target
    return Get-Item -LiteralPath $target
}

function Write-JsonWithoutBom
{
    param(
        [Parameter(Mandatory)]
        [object]$Value,

        [Parameter(Mandatory)]
        [string]$Path
    )

    [IO.File]::WriteAllText(
        $Path,
        ($Value | ConvertTo-Json -Depth 30) + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))
}

function Ensure-ReleaseNotesSigningDisclosure
{
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [ValidateSet('trusted-signed', 'unsigned')]
        [string]$Status
    )

    $notes = Get-Content -LiteralPath $Path -Raw
    if ($Status -eq 'unsigned' -and $notes -notmatch '(?i)unsigned|not code-signed')
    {
        $disclosure = @(
            '## Signing'
            ''
            'The Setup EXE is not code-signed. Windows may display a SmartScreen warning; verify the downloaded file against `SHA256SUMS.txt` before running it.'
        ) -join [Environment]::NewLine
        $updatedNotes = $notes.TrimEnd() + [Environment]::NewLine + [Environment]::NewLine + $disclosure + [Environment]::NewLine
        [IO.File]::WriteAllText($Path, $updatedNotes, [Text.UTF8Encoding]::new($false))
    }

    return Get-Item -LiteralPath $Path
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$versionMetadata = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Branding\version.json') -Raw | ConvertFrom-Json
$outputRoot = if ([IO.Path]::IsPathRooted($OutputDirectory))
{
    [IO.Path]::GetFullPath($OutputDirectory)
}
else
{
    [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputDirectory))
}

try
{
    if ([string]$versionMetadata.applicationVersion -cne $Version)
    {
        throw "Requested release version '$Version' does not match version.json '$($versionMetadata.applicationVersion)'."
    }
    if ([string]$versionMetadata.microsoftTerminalUpstreamRevision -cne $UpstreamRevision.ToLowerInvariant())
    {
        throw 'The supplied Microsoft Terminal upstream revision does not match version.json.'
    }
    $timestamp = [DateTimeOffset]::Parse(
        $BuildTimestamp,
        [Globalization.CultureInfo]::InvariantCulture,
        [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')

    if (Test-Path -LiteralPath $outputRoot)
    {
        throw "Release output directory already exists: $outputRoot"
    }
    New-Item -ItemType Directory -Path $outputRoot | Out-Null

    $installer = Get-Item -LiteralPath (Resolve-Path -LiteralPath $InstallerPath).Path
    $installerSignature = Get-AuthenticodeSignature -LiteralPath $installer.FullName
    if ($SigningStatus -eq 'trusted-signed' -and $installerSignature.Status -ne 'Valid')
    {
        throw "A trusted-signed release requires a valid installer signature; found $($installerSignature.Status)."
    }
    if ($SigningStatus -eq 'unsigned' -and $installerSignature.Status -eq 'Valid')
    {
        throw 'The installer is validly signed but the release was declared unsigned.'
    }

    $installerAsset = Copy-ReleaseFile -Source $installer.FullName -TargetName "winTerm-$Version-setup-x64.exe"
    $portableAsset = Copy-ReleaseFile -Source $PortablePath -TargetName "winTerm-$Version-portable-x64.zip"
    $noticesAsset = Copy-ReleaseFile -Source (Join-Path $repositoryRoot 'THIRD_PARTY_NOTICES.md') -TargetName 'THIRD_PARTY_NOTICES.md'

    $notesSource = if ([string]::IsNullOrWhiteSpace($ReleaseNotesPath))
    {
        Join-Path $repositoryRoot "docs\releases\$Version.md"
    }
    else
    {
        $ReleaseNotesPath
    }
    $notesAsset = Copy-ReleaseFile -Source $notesSource -TargetName "winTerm-$Version-release-notes.md"
    $notesAsset = Ensure-ReleaseNotesSigningDisclosure -Path $notesAsset.FullName -Status $SigningStatus

    $releaseInputs = @($installerAsset, $portableAsset, $noticesAsset, $notesAsset)
    $artifactRecords = @($releaseInputs | ForEach-Object {
        [ordered]@{
            name = $_.Name
            size = $_.Length
            sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    })

    $commit = $CommitSha.ToLowerInvariant()
    $upstream = $UpstreamRevision.ToLowerInvariant()
    $metadata = [ordered]@{
        schemaVersion = 2
        product = 'winTerm'
        publisher = 'helloThisWorld'
        productId = 'HelloThisWorld.winTerm'
        version = $Version
        packageVersion = [string]$versionMetadata.packageVersion
        channel = [string]$versionMetadata.channel
        tag = "v$Version"
        architecture = 'x64'
        distribution = @('inno-setup', 'portable-zip')
        signing = $SigningStatus
        installerSignatureStatus = [string]$installerSignature.Status
        commitSha = $commit
        microsoftTerminalUpstreamRevision = $upstream
        buildTimestamp = $timestamp
        workflowRunId = $WorkflowRunId
        artifacts = $artifactRecords
    }
    Write-JsonWithoutBom -Value $metadata -Path (Join-Path $outputRoot 'release-metadata.json')

    $spdx = [ordered]@{
        spdxVersion = 'SPDX-2.3'
        dataLicense = 'CC0-1.0'
        SPDXID = 'SPDXRef-DOCUMENT'
        name = "winTerm-$Version"
        documentNamespace = "https://github.com/HelloThisWorld/winTerm/sbom/$commit"
        creationInfo = [ordered]@{
            created = $timestamp
            creators = @('Organization: helloThisWorld', 'Tool: winTerm generate-release-artifacts.ps1')
        }
        packages = @(
            [ordered]@{
                name = 'winTerm'
                SPDXID = 'SPDXRef-Package-winTerm'
                versionInfo = $Version
                supplier = 'Organization: helloThisWorld'
                downloadLocation = 'NOASSERTION'
                filesAnalyzed = $false
                licenseConcluded = 'MIT'
                licenseDeclared = 'MIT'
                copyrightText = 'NOASSERTION'
                externalRefs = @(
                    [ordered]@{
                        referenceCategory = 'PACKAGE-MANAGER'
                        referenceType = 'purl'
                        referenceLocator = "pkg:github/HelloThisWorld/winTerm@$Version"
                    }
                )
            },
            [ordered]@{
                name = 'Microsoft Terminal upstream source'
                SPDXID = 'SPDXRef-Package-MicrosoftTerminal'
                versionInfo = $upstream
                supplier = 'Organization: Microsoft Corporation'
                downloadLocation = "https://github.com/microsoft/terminal/tree/$upstream"
                filesAnalyzed = $false
                licenseConcluded = 'MIT'
                licenseDeclared = 'MIT'
                copyrightText = 'NOASSERTION'
            }
        )
        relationships = @(
            [ordered]@{
                spdxElementId = 'SPDXRef-DOCUMENT'
                relationshipType = 'DESCRIBES'
                relatedSpdxElement = 'SPDXRef-Package-winTerm'
            },
            [ordered]@{
                spdxElementId = 'SPDXRef-Package-winTerm'
                relationshipType = 'VARIANT_OF'
                relatedSpdxElement = 'SPDXRef-Package-MicrosoftTerminal'
            }
        )
    }
    Write-JsonWithoutBom -Value $spdx -Path (Join-Path $outputRoot 'SBOM.spdx.json')

    $cycloneDx = [ordered]@{
        bomFormat = 'CycloneDX'
        specVersion = '1.5'
        serialNumber = "urn:uuid:$([guid]::NewGuid())"
        version = 1
        metadata = [ordered]@{
            timestamp = $timestamp
            tools = @([ordered]@{ vendor = 'helloThisWorld'; name = 'winTerm release tooling' })
            component = [ordered]@{
                type = 'application'
                'bom-ref' = 'pkg:github/HelloThisWorld/winTerm'
                group = 'HelloThisWorld'
                name = 'winTerm'
                version = $Version
                publisher = 'helloThisWorld'
                licenses = @([ordered]@{ license = [ordered]@{ id = 'MIT' } })
                purl = "pkg:github/HelloThisWorld/winTerm@$Version"
            }
        }
        components = @(
            [ordered]@{
                type = 'library'
                'bom-ref' = 'github:microsoft/terminal'
                group = 'microsoft'
                name = 'terminal'
                version = $upstream
                licenses = @([ordered]@{ license = [ordered]@{ id = 'MIT' } })
                externalReferences = @([ordered]@{ type = 'vcs'; url = "https://github.com/microsoft/terminal/tree/$upstream" })
            }
        )
        dependencies = @(
            [ordered]@{ ref = 'pkg:github/HelloThisWorld/winTerm'; dependsOn = @('github:microsoft/terminal') },
            [ordered]@{ ref = 'github:microsoft/terminal'; dependsOn = @() }
        )
    }
    Write-JsonWithoutBom -Value $cycloneDx -Path (Join-Path $outputRoot 'SBOM.cyclonedx.json')

    & (Join-Path $PSScriptRoot 'generate-checksums.ps1') -Directory $outputRoot
    if (-not $?)
    {
        throw 'Release checksum generation failed.'
    }
    Write-Host "Generated allowlisted winTerm $Version release assets: $outputRoot" -ForegroundColor Green
}
catch
{
    Write-Error "Release artifact generation failed: $($_.Exception.Message)"
    exit 1
}
