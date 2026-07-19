# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Directory,

    [Parameter()]
    [ValidateSet('x64', 'arm64')]
    [string[]]$Architecture = @('x64'),

    [Parameter()]
    [switch]$RequireSigned,

    [Parameter()]
    [switch]$RequireSelfSigned,

    [Parameter()]
    [string]$ExpectedPublisher
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

function Resolve-WindowsSdkTool
{
    param(
        [Parameter(Mandatory)]
        [string]$Name
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -ne $command)
    {
        return $command.Source
    }

    $sdkBinRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    if (Test-Path -LiteralPath $sdkBinRoot)
    {
        foreach ($version in @(Get-ChildItem -LiteralPath $sdkBinRoot -Directory |
            Sort-Object Name -Descending))
        {
            $candidate = Join-Path $version.FullName "x64\$Name"
            if (Test-Path -LiteralPath $candidate -PathType Leaf)
            {
                return $candidate
            }
        }
    }

    throw "$Name was not found. Install the Windows SDK declared in .vsconfig."
}

function Test-TrustedSignature
{
    param(
        [Parameter(Mandatory)]
        [System.IO.FileInfo]$Artifact
    )

    $signature = Get-AuthenticodeSignature -LiteralPath $Artifact.FullName
    Assert-Condition ($signature.Status -eq 'Valid') "$($Artifact.Name) has a valid trusted signature"
    Assert-Condition ($null -ne $signature.SignerCertificate) "$($Artifact.Name) contains a signer certificate"
    Assert-Condition ($null -ne $signature.TimeStamperCertificate) "$($Artifact.Name) signature is timestamped"
    if (-not [string]::IsNullOrWhiteSpace($ExpectedPublisher))
    {
        Assert-Condition ($signature.SignerCertificate.Subject -ceq $ExpectedPublisher) "$($Artifact.Name) signer matches the protected publisher"
    }

    $signTool = Resolve-WindowsSdkTool -Name 'signtool.exe'
    & $signTool verify /pa /all /v $Artifact.FullName
    if ($LASTEXITCODE -ne 0)
    {
        throw "signtool.exe trust verification failed for '$($Artifact.Name)' with exit code $LASTEXITCODE."
    }
    Write-Host "PASS: signtool.exe verified $($Artifact.Name)" -ForegroundColor Green
}

function Test-SelfSignedSignature
{
    param(
        [Parameter(Mandatory)]
        [System.IO.FileInfo]$Artifact,

        [Parameter(Mandatory)]
        [string]$SignaturePath,

        [Parameter(Mandatory)]
        [Security.Cryptography.X509Certificates.X509Certificate2]$Certificate
    )

    try
    {
        Add-Type -AssemblyName System.Security.Cryptography.Pkcs -ErrorAction Stop
    }
    catch
    {
        Add-Type -AssemblyName System.Security
    }

    $signatureBytes = [IO.File]::ReadAllBytes($SignaturePath)
    Assert-Condition (
        $signatureBytes.Length -gt 4 -and
        [Text.Encoding]::ASCII.GetString($signatureBytes, 0, 4) -ceq 'PKCX'
    ) "$($Artifact.Name) contains a valid MSIX signature header"

    $cmsBytes = New-Object byte[] ($signatureBytes.Length - 4)
    [Array]::Copy($signatureBytes, 4, $cmsBytes, 0, $cmsBytes.Length)
    $cms = New-Object Security.Cryptography.Pkcs.SignedCms
    $cms.Decode($cmsBytes)
    $cms.CheckSignature($true)
    Assert-Condition ($cms.SignerInfos.Count -eq 1) "$($Artifact.Name) contains exactly one signer"
    Assert-Condition (
        $cms.SignerInfos[0].Certificate.Thumbprint -ceq $Certificate.Thumbprint
    ) "$($Artifact.Name) signer matches the published certificate"

    $signature = Get-AuthenticodeSignature -LiteralPath $Artifact.FullName
    Assert-Condition ($null -ne $signature.SignerCertificate) "$($Artifact.Name) contains an Authenticode signer"
    Assert-Condition (
        $signature.SignerCertificate.Thumbprint -ceq $Certificate.Thumbprint
    ) "$($Artifact.Name) Authenticode signer matches the published certificate"
}

function Test-MsixPackage
{
    param(
        [Parameter(Mandatory)]
        [System.IO.FileInfo]$Package,

        [Parameter(Mandatory)]
        [string]$ExpectedArchitecture,

        [Parameter(Mandatory)]
        [string]$TemporaryRoot,

        [Parameter()]
        [Security.Cryptography.X509Certificates.X509Certificate2]$SelfSignedCertificate
    )

    $makeAppx = Resolve-WindowsSdkTool -Name 'makeappx.exe'

    $unpackDirectory = Join-Path $TemporaryRoot $ExpectedArchitecture
    New-Item -ItemType Directory -Path $unpackDirectory | Out-Null
    & $makeAppx unpack /p $Package.FullName /d $unpackDirectory /o
    if ($LASTEXITCODE -ne 0)
    {
        throw "makeappx.exe failed to unpack '$($Package.Name)'."
    }

    [xml]$manifest = Get-Content -LiteralPath (Join-Path $unpackDirectory 'AppxManifest.xml') -Raw
    $namespace = [System.Xml.XmlNamespaceManager]::new($manifest.NameTable)
    $namespace.AddNamespace('f', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
    $namespace.AddNamespace('uap3', 'http://schemas.microsoft.com/appx/manifest/uap/windows10/3')
    $namespace.AddNamespace('desktop', 'http://schemas.microsoft.com/appx/manifest/desktop/windows10')
    $identity = $manifest.SelectSingleNode('/f:Package/f:Identity', $namespace)
    $aliases = @($manifest.SelectNodes('//uap3:AppExecutionAlias/desktop:ExecutionAlias', $namespace) | ForEach-Object { $_.Alias })

    Assert-Condition ($identity.Name -eq 'HelloThisWorld.winTerm') "$($Package.Name) uses package identity HelloThisWorld.winTerm"
    Assert-Condition ($identity.Version -eq '1.0.2.0') "$($Package.Name) contains package version 1.0.2.0"
    Assert-Condition ($identity.ProcessorArchitecture.ToLowerInvariant() -eq $ExpectedArchitecture) "$($Package.Name) contains architecture $ExpectedArchitecture"
    Assert-Condition ($aliases -contains 'winterm.exe') "$($Package.Name) registers winterm.exe"
    Assert-Condition ($aliases -notcontains 'wt.exe') "$($Package.Name) does not register wt.exe"
    Assert-Condition (-not (Test-Path -LiteralPath (Join-Path $unpackDirectory 'wt.exe'))) "$($Package.Name) does not contain wt.exe"

    if (-not [string]::IsNullOrWhiteSpace($ExpectedPublisher))
    {
        Assert-Condition ($identity.Publisher -ceq $ExpectedPublisher) "$($Package.Name) publisher matches the protected publisher"
    }

    if ($RequireSigned)
    {
        Test-TrustedSignature -Artifact $Package
    }
    elseif ($RequireSelfSigned)
    {
        Test-SelfSignedSignature `
            -Artifact $Package `
            -SignaturePath (Join-Path $unpackDirectory 'AppxSignature.p7x') `
            -Certificate $SelfSignedCertificate
    }
    else
    {
        $signature = Get-AuthenticodeSignature -LiteralPath $Package.FullName
        Write-Host "INFO: $($Package.Name) signature status is $($signature.Status)."
    }
}

function Test-MsixBundle
{
    param(
        [Parameter(Mandatory)]
        [System.IO.FileInfo]$Bundle,

        [Parameter(Mandatory)]
        [string]$TemporaryRoot,

        [Parameter()]
        [Security.Cryptography.X509Certificates.X509Certificate2]$SelfSignedCertificate
    )

    $makeAppx = Resolve-WindowsSdkTool -Name 'makeappx.exe'

    $unpackDirectory = Join-Path $TemporaryRoot 'bundle'
    New-Item -ItemType Directory -Path $unpackDirectory | Out-Null
    & $makeAppx unpack /p $Bundle.FullName /d $unpackDirectory /o
    if ($LASTEXITCODE -ne 0)
    {
        throw "makeappx.exe failed to unpack '$($Bundle.Name)'."
    }

    [xml]$manifest = Get-Content -LiteralPath (Join-Path $unpackDirectory 'AppxMetadata\AppxBundleManifest.xml') -Raw
    $namespace = [System.Xml.XmlNamespaceManager]::new($manifest.NameTable)
    $namespace.AddNamespace('b', 'http://schemas.microsoft.com/appx/2013/bundle')
    $identity = $manifest.SelectSingleNode('/b:Bundle/b:Identity', $namespace)
    $packages = @($manifest.SelectNodes('/b:Bundle/b:Packages/b:Package', $namespace))
    $architectures = @($packages | ForEach-Object { $_.Architecture.ToLowerInvariant() })

    Assert-Condition ($identity.Name -eq 'HelloThisWorld.winTerm') "$($Bundle.Name) uses bundle identity HelloThisWorld.winTerm"
    Assert-Condition ($identity.Version -eq '1.0.2.0') "$($Bundle.Name) contains bundle version 1.0.2.0"
    if (-not [string]::IsNullOrWhiteSpace($ExpectedPublisher))
    {
        Assert-Condition ($identity.Publisher -ceq $ExpectedPublisher) "$($Bundle.Name) publisher matches the protected publisher"
    }
    Assert-Condition ($packages.Count -eq 2) "$($Bundle.Name) contains exactly two application packages"
    Assert-Condition (((($architectures | Sort-Object) -join ',') -ceq 'arm64,x64')) "$($Bundle.Name) contains exactly x64 and ARM64"
    foreach ($package in $packages)
    {
        Assert-Condition (Test-Path -LiteralPath (Join-Path $unpackDirectory $package.FileName) -PathType Leaf) "$($Bundle.Name) contains $($package.FileName)"
    }

    if ($RequireSigned)
    {
        Test-TrustedSignature -Artifact $Bundle
    }
    elseif ($RequireSelfSigned)
    {
        Test-SelfSignedSignature `
            -Artifact $Bundle `
            -SignaturePath (Join-Path $unpackDirectory 'AppxSignature.p7x') `
            -Certificate $SelfSignedCertificate
    }
    else
    {
        $signature = Get-AuthenticodeSignature -LiteralPath $Bundle.FullName
        Write-Host "INFO: $($Bundle.Name) signature status is $($signature.Status)."
    }
}

$temporaryDirectory = $null
$selfSignedCertificate = $null

try
{
    $root = (Resolve-Path -LiteralPath $Directory).Path
    if ($RequireSigned -and $RequireSelfSigned)
    {
        throw 'RequireSigned and RequireSelfSigned cannot be enabled together.'
    }
    if ($RequireSigned -and [string]::IsNullOrWhiteSpace($ExpectedPublisher))
    {
        throw 'ExpectedPublisher is required when RequireSigned is enabled.'
    }
    if ($RequireSelfSigned -and [string]::IsNullOrWhiteSpace($ExpectedPublisher))
    {
        throw 'ExpectedPublisher is required when RequireSelfSigned is enabled.'
    }
    $required = [System.Collections.Generic.List[string]]::new()
    foreach ($name in @(
        'SHA256SUMS.txt',
        'THIRD_PARTY_NOTICES.md',
        'SBOM.spdx.json',
        'SBOM.cyclonedx.json',
        'release-metadata.json',
        'winTerm-1.0.2-release-notes.md',
        'winTerm-1.0.2-symbols.zip'
    ))
    {
        [void]$required.Add($name)
    }
    foreach ($arch in $Architecture)
    {
        [void]$required.Add("winTerm-1.0.2-$arch.msix")
    }
    if ($Architecture.Count -eq 2)
    {
        [void]$required.Add('winTerm-1.0.2.msixbundle')
    }
    if ($RequireSelfSigned)
    {
        [void]$required.Add('winTerm-1.0.2.cer')
        [void]$required.Add('INSTALL.txt')
    }

    $allowed = [System.Collections.Generic.HashSet[string]]::new($required, [StringComparer]::OrdinalIgnoreCase)
    foreach ($item in Get-ChildItem -LiteralPath $root -Force)
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
    Assert-Condition (((($checksumNames | Sort-Object) -join "`n") -ceq (($expectedChecksumNames | Sort-Object) -join "`n"))) 'SHA256SUMS.txt covers every release asset except itself'

    $releaseNotes = Get-Content -LiteralPath (Join-Path $root 'winTerm-1.0.2-release-notes.md') -Raw
    Assert-Condition ($releaseNotes.Contains('# winTerm 1.0.2')) 'Release notes contain the stable release title'

    $metadata = Get-Content -LiteralPath (Join-Path $root 'release-metadata.json') -Raw | ConvertFrom-Json
    Assert-Condition ($metadata.version -eq '1.0.2') 'Release metadata version is 1.0.2'
    Assert-Condition ($metadata.packageVersion -eq '1.0.2.0') 'Release metadata package version is 1.0.2.0'
    Assert-Condition ($metadata.channel -eq 'stable') 'Release metadata channel is stable'
    Assert-Condition ($metadata.commitSha -match '^[0-9a-f]{40}$') 'Release metadata contains a full commit SHA'
    Assert-Condition ($metadata.microsoftTerminalUpstreamRevision -match '^[0-9a-f]{40}$') 'Release metadata contains the upstream revision'

    foreach ($sbomName in @('SBOM.spdx.json', 'SBOM.cyclonedx.json'))
    {
        $sbomText = Get-Content -LiteralPath (Join-Path $root $sbomName) -Raw
        $null = $sbomText | ConvertFrom-Json
        Assert-Condition ($sbomText -notmatch '[A-Za-z]:\\\\Users\\\\') "$sbomName contains no Windows user path"
        Assert-Condition ($sbomText -notmatch '(?i)(certificatePassword|github_token|BEGIN PRIVATE KEY)') "$sbomName contains no signing or repository secret"
    }

    if ($RequireSelfSigned)
    {
        Assert-Condition ($metadata.signing -eq 'self-signed') 'Release metadata declares self-signed distribution'
        $certificatePath = Join-Path $root 'winTerm-1.0.2.cer'
        $selfSignedCertificate = [Security.Cryptography.X509Certificates.X509Certificate2]::new(
            $certificatePath)
        Assert-Condition (-not $selfSignedCertificate.HasPrivateKey) 'Published certificate does not contain a private key'
        Assert-Condition ($selfSignedCertificate.Subject -ceq $ExpectedPublisher) 'Published certificate subject matches the package publisher'
        Assert-Condition ($selfSignedCertificate.NotAfter.ToUniversalTime() -gt [DateTime]::UtcNow) 'Published certificate is not expired'
        $installationText = Get-Content -LiteralPath (Join-Path $root 'INSTALL.txt') -Raw
        Assert-Condition ($installationText.Contains('winTerm-1.0.2.cer')) 'Installation instructions name the published certificate'
        Assert-Condition ($installationText.Contains('winTerm-1.0.2-x64.msix')) 'Installation instructions name the x64 installer'
    }

    $temporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) ("winterm-release-verify-{0}" -f [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null
    foreach ($arch in $Architecture)
    {
        $package = Get-Item -LiteralPath (Join-Path $root "winTerm-1.0.2-$arch.msix")
        Test-MsixPackage `
            -Package $package `
            -ExpectedArchitecture $arch `
            -TemporaryRoot $temporaryDirectory `
            -SelfSignedCertificate $selfSignedCertificate
    }
    if ($Architecture.Count -eq 2)
    {
        $bundle = Get-Item -LiteralPath (Join-Path $root 'winTerm-1.0.2.msixbundle')
        Test-MsixBundle `
            -Bundle $bundle `
            -TemporaryRoot $temporaryDirectory `
            -SelfSignedCertificate $selfSignedCertificate
    }

    Write-Host 'winTerm release asset verification passed.' -ForegroundColor Green
}
catch
{
    Write-Error "Release asset verification failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    if ($null -ne $temporaryDirectory -and (Test-Path -LiteralPath $temporaryDirectory))
    {
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
    if ($null -ne $selfSignedCertificate)
    {
        $selfSignedCertificate.Dispose()
    }
}
