# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Directory,

    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$')]
    [string]$Version,

    [Parameter()]
    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [switch]$RequireSigned
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-Condition
{
    param(
        [Parameter(Mandatory)]
        [bool]$Condition,

        [Parameter(Mandatory)]
        [string]$Message
    )

    if (-not $Condition)
    {
        throw $Message
    }
    Write-Host "PASS: $Message" -ForegroundColor Green
}

try
{
    $root = (Resolve-Path -LiteralPath $Directory).Path
    $platformLabel = $Platform.ToLowerInvariant()
    $installerName = "winTerm-$Version-setup-$platformLabel.exe"
    $portableName = "winTerm-$Version-portable-$platformLabel.zip"
    $notesName = "winTerm-$Version-release-notes.md"
    $required = @(
        $installerName,
        $portableName,
        'SHA256SUMS.txt',
        'THIRD_PARTY_NOTICES.md',
        'SBOM.spdx.json',
        'SBOM.cyclonedx.json',
        'release-metadata.json',
        $notesName
    )
    $allowed = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($name in $required)
    {
        [void]$allowed.Add($name)
    }

    $entries = @(Get-ChildItem -LiteralPath $root -Force)
    Assert-Condition ($entries.Count -eq $required.Count) 'Release directory contains the exact allowlisted asset count'
    foreach ($item in $entries)
    {
        Assert-Condition (-not $item.PSIsContainer) "Release entry '$($item.Name)' is a file"
        Assert-Condition (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) "Release entry '$($item.Name)' is not a reparse point"
        Assert-Condition ($allowed.Contains($item.Name)) "Release entry '$($item.Name)' is allowlisted"
        Assert-Condition ($item.Length -gt 0) "Release entry '$($item.Name)' is not empty"
    }
    foreach ($name in $required)
    {
        Assert-Condition (Test-Path -LiteralPath (Join-Path $root $name) -PathType Leaf) "Required release asset exists: $name"
    }

    & (Join-Path $PSScriptRoot 'verify-checksums.ps1') -Directory $root
    if (-not $?)
    {
        throw 'Release checksum verification failed.'
    }
    $checksumNames = @(Get-Content -LiteralPath (Join-Path $root 'SHA256SUMS.txt') | ForEach-Object {
        if ($_ -notmatch '^[0-9a-f]{64}  (?<name>[^\\/]+)$')
        {
            throw 'SHA256SUMS.txt contains invalid syntax.'
        }
        $Matches.name
    })
    $expectedChecksumNames = @($required | Where-Object { $_ -ne 'SHA256SUMS.txt' } | Sort-Object)
    Assert-Condition ((($checksumNames | Sort-Object) -join "`n") -ceq (($expectedChecksumNames | Sort-Object) -join "`n")) 'SHA256SUMS.txt covers every release asset except itself'

    & (Join-Path $PSScriptRoot 'verify-release-layout.ps1') -Path (Join-Path $root $installerName) -Type Installer -Platform $Platform -Version $Version
    if (-not $?) { throw 'Installer layout verification failed.' }
    & (Join-Path $PSScriptRoot 'verify-release-layout.ps1') -Path (Join-Path $root $portableName) -Type Portable -Platform $Platform -Version $Version
    if (-not $?) { throw 'Portable layout verification failed.' }

    $signature = Get-AuthenticodeSignature -LiteralPath (Join-Path $root $installerName)
    if ($RequireSigned)
    {
        Assert-Condition ($signature.Status -eq 'Valid') 'Installer has a valid trusted Authenticode signature'
        Assert-Condition ($null -ne $signature.SignerCertificate) 'Installer contains a signer certificate'
        Assert-Condition ($null -ne $signature.TimeStamperCertificate) 'Installer signature is timestamped'
    }
    else
    {
        Assert-Condition ($signature.Status -in @('Valid', 'NotSigned')) 'Installer is validly signed or explicitly unsigned'
    }

    $metadata = Get-Content -LiteralPath (Join-Path $root 'release-metadata.json') -Raw | ConvertFrom-Json
    Assert-Condition ($metadata.schemaVersion -eq 2) 'Release metadata schema is version 2'
    Assert-Condition ($metadata.product -ceq 'winTerm') 'Release metadata product is winTerm'
    Assert-Condition ($metadata.publisher -ceq 'helloThisWorld') 'Release metadata publisher is helloThisWorld'
    Assert-Condition ($metadata.productId -ceq 'HelloThisWorld.winTerm') 'Release metadata product ID is HelloThisWorld.winTerm'
    Assert-Condition ($metadata.version -ceq $Version) "Release metadata version is $Version"
    Assert-Condition ($metadata.architecture -ceq $platformLabel) "Release metadata architecture is $platformLabel"
    Assert-Condition ($metadata.commitSha -match '^[0-9a-f]{40}$') 'Release metadata contains a full commit SHA'
    Assert-Condition ($metadata.microsoftTerminalUpstreamRevision -match '^[0-9a-f]{40}$') 'Release metadata contains the upstream revision'
    Assert-Condition ((@($metadata.distribution) -join ',') -ceq 'inno-setup,portable-zip') 'Release metadata declares only EXE and Portable distributions'
    if ($RequireSigned)
    {
        Assert-Condition ($metadata.signing -ceq 'trusted-signed') 'Release metadata declares trusted signing'
    }
    elseif ($signature.Status -eq 'NotSigned')
    {
        Assert-Condition ($metadata.signing -ceq 'unsigned') 'Release metadata explicitly declares the unsigned installer'
    }

    $notes = Get-Content -LiteralPath (Join-Path $root $notesName) -Raw
    Assert-Condition ($notes.Contains("# winTerm $Version")) 'Release notes contain the matching title'
    if ($metadata.signing -eq 'unsigned')
    {
        Assert-Condition ($notes -match '(?i)unsigned|not code-signed') 'Release notes disclose the unsigned installer'
    }

    foreach ($sbomName in @('SBOM.spdx.json', 'SBOM.cyclonedx.json'))
    {
        $sbomText = Get-Content -LiteralPath (Join-Path $root $sbomName) -Raw
        $sbom = $sbomText | ConvertFrom-Json
        Assert-Condition ($null -ne $sbom) "$sbomName contains valid JSON"
        Assert-Condition ($sbomText -notmatch '[A-Za-z]:\\\\Users\\\\') "$sbomName contains no Windows user path"
        Assert-Condition ($sbomText -notmatch '(?i)(certificatePassword|github_token|BEGIN PRIVATE KEY)') "$sbomName contains no signing or repository secret"
    }

    Write-Host 'winTerm release asset verification passed.' -ForegroundColor Green
}
catch
{
    Write-Error "Release asset verification failed: $($_.Exception.Message)"
    exit 1
}
