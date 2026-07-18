# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [string]$OutputDirectory,

    [Parameter()]
    [string]$PackagePath,

    [Parameter()]
    [string]$SignToolPath,

    [Parameter()]
    [string]$MakeAppxPath,

    [Parameter()]
    [switch]$IncludeTests,

    [Parameter()]
    [switch]$ReleaseAsset
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Resolve-WindowsSdkTool
{
    param(
        [Parameter(Mandatory)]
        [string]$Name,

        [Parameter()]
        [string]$ExplicitPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath))
    {
        $explicitItem = Get-Item -LiteralPath (Resolve-Path -LiteralPath $ExplicitPath).Path
        if ($explicitItem.PSIsContainer)
        {
            throw "Windows SDK tool '$ExplicitPath' is a directory."
        }
        return $explicitItem.FullName
    }

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -ne $command)
    {
        return $command.Source
    }

    $sdkBinRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    if (Test-Path -LiteralPath $sdkBinRoot)
    {
        $versions = @(Get-ChildItem -LiteralPath $sdkBinRoot -Directory |
            Sort-Object Name -Descending)
        foreach ($version in $versions)
        {
            $candidate = Join-Path $version.FullName "x64\$Name"
            if (Test-Path -LiteralPath $candidate -PathType Leaf)
            {
                return $candidate
            }
        }
    }

    throw "$Name was not found. Install the Windows SDK declared in .vsconfig or pass an explicit tool path."
}

function Invoke-NativeTool
{
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string[]]$Arguments,

        [Parameter(Mandatory)]
        [string]$Description
    )

    & $Path @Arguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

function Assert-WinTermPackageManifest
{
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    [xml]$manifest = Get-Content -LiteralPath $Path -Raw
    $identity = $manifest.SelectSingleNode(
        '/*[local-name()="Package"]/*[local-name()="Identity"]')
    if ($null -eq $identity)
    {
        throw 'The package manifest does not contain an Identity element.'
    }
    if ($identity.Name -cne 'HelloThisWorld.winTerm')
    {
        throw "Unexpected package identity '$($identity.Name)'."
    }
    if ($identity.Publisher -cne 'CN=winTerm Development')
    {
        throw "Unexpected package publisher '$($identity.Publisher)'."
    }
    if ($identity.Version -cne $script:packageVersion)
    {
        throw "Unexpected package version '$($identity.Version)'."
    }
    if ($identity.ProcessorArchitecture -cne 'x64')
    {
        throw "Unexpected package architecture '$($identity.ProcessorArchitecture)'."
    }

    $aliases = @($manifest.SelectNodes(
        '//*[local-name()="AppExecutionAlias"]/*[local-name()="ExecutionAlias"]') |
        ForEach-Object { $_.Alias })
    if ($aliases -notcontains 'winterm.exe' -or $aliases -contains 'wt.exe')
    {
        throw 'The winTerm package must register winterm.exe and must not register wt.exe.'
    }
}

function Assert-CryptographicPackageSignature
{
    param(
        [Parameter(Mandatory)]
        [string]$SignaturePath,

        [Parameter(Mandatory)]
        [string]$ExpectedThumbprint
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
    if ($signatureBytes.Length -le 4 -or
        [Text.Encoding]::ASCII.GetString($signatureBytes, 0, 4) -cne 'PKCX')
    {
        throw 'The MSIX signature part has an invalid header.'
    }

    $cmsBytes = New-Object byte[] ($signatureBytes.Length - 4)
    [Array]::Copy($signatureBytes, 4, $cmsBytes, 0, $cmsBytes.Length)
    $cms = New-Object Security.Cryptography.Pkcs.SignedCms
    $cms.Decode($cmsBytes)
    $cms.CheckSignature($true)

    if ($cms.SignerInfos.Count -ne 1)
    {
        throw "Expected one MSIX signer, found $($cms.SignerInfos.Count)."
    }
    if ($cms.SignerInfos[0].Certificate.Thumbprint -cne $ExpectedThumbprint)
    {
        throw 'The MSIX signer does not match the exported development certificate.'
    }
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$releaseMetadata = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Branding\version.json') -Raw |
    ConvertFrom-Json
$applicationVersion = [string]$releaseMetadata.applicationVersion
$packageVersion = [string]$releaseMetadata.packageVersion
$packageOutputRoot = Join-Path $repositoryRoot 'src\cascadia\CascadiaPackage\AppPackages'
$temporaryDirectory = $null
$gitExcludesPath = $null
$certificateThumbprint = $null
$createdOutputDirectory = $false
$completed = $false

try
{
    if ($PSVersionTable.PSVersion.Major -lt 7 -and
        [string]::IsNullOrWhiteSpace($PackagePath))
    {
        throw 'PowerShell 7 or later is required when building the package.'
    }
    if (-not [string]::IsNullOrWhiteSpace($PackagePath) -and $IncludeTests)
    {
        throw 'IncludeTests cannot be combined with PackagePath because the package was built separately.'
    }
    if ($null -eq (Get-Command New-SelfSignedCertificate -ErrorAction SilentlyContinue))
    {
        throw 'New-SelfSignedCertificate is unavailable on this Windows installation.'
    }

    $makeAppx = Resolve-WindowsSdkTool -Name 'makeappx.exe' -ExplicitPath $MakeAppxPath
    $signTool = Resolve-WindowsSdkTool -Name 'signtool.exe' -ExplicitPath $SignToolPath
    $toolDirectories = @((Split-Path -Parent $makeAppx), (Split-Path -Parent $signTool)) |
        Select-Object -Unique
    $env:PATH = (($toolDirectories + @($env:PATH)) -join [IO.Path]::PathSeparator)

    & (Join-Path $PSScriptRoot 'verify-version.ps1')
    if (-not $?)
    {
        throw 'Version validation failed.'
    }
    & (Join-Path $PSScriptRoot 'test.ps1') -Suite Smoke -Configuration Release -Platform x64
    if (-not $?)
    {
        throw 'Smoke validation failed.'
    }

    $sourcePackage = $null
    if ([string]::IsNullOrWhiteSpace($PackagePath))
    {
        $buildArguments = @{
            Configuration = 'Release'
            Platform = 'x64'
            GeneratePackage = $true
        }
        if ($IncludeTests)
        {
            $buildArguments.IncludeTests = $true
        }

        & (Join-Path $PSScriptRoot 'build.ps1') @buildArguments
        if (-not $?)
        {
            throw 'Release package build failed.'
        }
        & (Join-Path $PSScriptRoot 'package.ps1') -Platform x64 -SkipBuild
        if (-not $?)
        {
            throw 'Package validation failed.'
        }

        $sourcePackage = Get-ChildItem -LiteralPath $packageOutputRoot -Recurse -File -Filter '*.msix' |
            Where-Object { $_.FullName -match '(?i)x64' } |
            Sort-Object LastWriteTimeUtc -Descending |
            Select-Object -First 1
        if ($null -eq $sourcePackage)
        {
            throw "No x64 MSIX was found under '$packageOutputRoot'."
        }

        if ($IncludeTests)
        {
            & (Join-Path $PSScriptRoot 'test.ps1') -Suite Relevant -Configuration Release -Platform x64
            if (-not $?)
            {
                throw 'Relevant compiled tests failed.'
            }
        }
    }
    else
    {
        $sourcePackage = Get-Item -LiteralPath (Resolve-Path -LiteralPath $PackagePath).Path
    }

    if ($sourcePackage.PSIsContainer -or $sourcePackage.Extension -cne '.msix')
    {
        throw "Package input '$($sourcePackage.FullName)' is not an MSIX file."
    }
    if (($sourcePackage.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)
    {
        throw "Package input '$($sourcePackage.FullName)' is a reparse point."
    }
    if ($sourcePackage.Length -le 0)
    {
        throw "Package input '$($sourcePackage.FullName)' is empty."
    }
    $existingSignature = Get-AuthenticodeSignature -LiteralPath $sourcePackage.FullName
    if ($null -ne $existingSignature.SignerCertificate)
    {
        throw 'The source MSIX is already signed. Refusing to replace or append a signature.'
    }

    $defaultOutputName = if ($ReleaseAsset)
    {
        "winTerm-$applicationVersion-self-signed-x64"
    }
    else
    {
        "winTerm-$applicationVersion-development-x64"
    }
    $outputRoot = if ([string]::IsNullOrWhiteSpace($OutputDirectory))
    {
        Join-Path $repositoryRoot "artifacts\local\$defaultOutputName"
    }
    elseif ([IO.Path]::IsPathRooted($OutputDirectory))
    {
        [IO.Path]::GetFullPath($OutputDirectory)
    }
    else
    {
        [IO.Path]::GetFullPath((Join-Path (Get-Location).Path $OutputDirectory))
    }
    if (Test-Path -LiteralPath $outputRoot)
    {
        throw "Output directory already exists: $outputRoot"
    }
    New-Item -ItemType Directory -Path $outputRoot | Out-Null
    $createdOutputDirectory = $true

    $signedPackageName = if ($ReleaseAsset)
    {
        "winTerm-$applicationVersion-x64.msix"
    }
    else
    {
        "winTerm-$applicationVersion-development-x64.msix"
    }
    $certificateName = if ($ReleaseAsset)
    {
        "winTerm-$applicationVersion.cer"
    }
    else
    {
        "winTerm-$applicationVersion-development.cer"
    }
    $signedPackagePath = Join-Path $outputRoot $signedPackageName
    $certificatePath = Join-Path $outputRoot $certificateName
    Copy-Item -LiteralPath $sourcePackage.FullName -Destination $signedPackagePath

    $certificate = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject 'CN=winTerm Development' `
        -FriendlyName $(if ($ReleaseAsset) { 'winTerm Self-Signed Release Signing' } else { 'winTerm Local Development Signing' }) `
        -CertStoreLocation 'Cert:\CurrentUser\My' `
        -KeyAlgorithm RSA `
        -KeyLength 3072 `
        -KeyExportPolicy NonExportable `
        -HashAlgorithm SHA256 `
        -NotAfter (Get-Date).AddYears(1)
    $certificateThumbprint = $certificate.Thumbprint
    Export-Certificate -Cert $certificate -FilePath $certificatePath | Out-Null

    Invoke-NativeTool `
        -Path $signTool `
        -Arguments @('sign', '/fd', 'SHA256', '/s', 'My', '/sha1', $certificateThumbprint, $signedPackagePath) `
        -Description 'Self-signed MSIX signing'

    $temporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) (
        "winterm-self-signed-msix-{0}" -f [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null
    Invoke-NativeTool `
        -Path $makeAppx `
        -Arguments @('unpack', '/p', $signedPackagePath, '/d', $temporaryDirectory, '/o') `
        -Description 'Signed MSIX inspection'

    Assert-WinTermPackageManifest -Path (Join-Path $temporaryDirectory 'AppxManifest.xml')
    Assert-CryptographicPackageSignature `
        -SignaturePath (Join-Path $temporaryDirectory 'AppxSignature.p7x') `
        -ExpectedThumbprint $certificateThumbprint

    $publicCertificate = [Security.Cryptography.X509Certificates.X509Certificate2]::new(
        $certificatePath)
    if ($publicCertificate.HasPrivateKey)
    {
        throw 'The exported development certificate unexpectedly contains a private key.'
    }
    $embeddedSignature = Get-AuthenticodeSignature -LiteralPath $signedPackagePath
    if ($null -eq $embeddedSignature.SignerCertificate -or
        $embeddedSignature.SignerCertificate.Thumbprint -cne $certificateThumbprint)
    {
        throw 'The embedded MSIX signer does not match the exported development certificate.'
    }

    $repositoryGitPath = $repositoryRoot.Replace('\', '/')
    $gitSafeDirectoryArgument = "safe.directory=$repositoryGitPath"
    $gitExcludesPath = Join-Path ([IO.Path]::GetTempPath()) (
        "winterm-empty-git-excludes-{0}" -f [guid]::NewGuid().ToString('N'))
    [IO.File]::WriteAllText($gitExcludesPath, [string]::Empty)
    $gitExcludesGitPath = $gitExcludesPath.Replace('\', '/')
    $gitArguments = @(
        '-c',
        $gitSafeDirectoryArgument,
        '-c',
        "core.excludesfile=$gitExcludesGitPath")
    $commitOutput = @(& git @gitArguments -C $repositoryRoot rev-parse HEAD)
    if ($LASTEXITCODE -ne 0)
    {
        throw "Git commit discovery failed with exit code $LASTEXITCODE."
    }
    $commitSha = ($commitOutput -join '').Trim()
    if ($commitSha -notmatch '^[0-9a-fA-F]{40}$')
    {
        throw "Git returned an invalid commit SHA '$commitSha'."
    }
    $statusOutput = @(& git @gitArguments -C $repositoryRoot status --porcelain)
    if ($LASTEXITCODE -ne 0)
    {
        throw "Git working tree discovery failed with exit code $LASTEXITCODE."
    }
    $workingTreeDirty = -not [string]::IsNullOrWhiteSpace(
        ($statusOutput -join [Environment]::NewLine))

    $packageDescription = if ($ReleaseAsset)
    {
        @'
This is the official GitHub self-signed release package. It is not signed by
a public certificate authority. Verify the GitHub Release URL and hashes
before trusting only the included public certificate.
'@
    }
    else
    {
        @'
This package is self-signed for local or controlled internal testing.
It is not signed by a public certificate authority or intended for public
redistribution.
'@
    }
    $instructions = @'
winTerm __APPLICATION_VERSION__ x64 Self-Signed Package
=======================================================

__PACKAGE_DESCRIPTION__

Source commit: __COMMIT_SHA__
Working tree dirty when packaged: __WORKING_TREE_DIRTY__
Certificate subject: CN=winTerm Development
Certificate thumbprint: __CERTIFICATE_THUMBPRINT__
Certificate expires: __CERTIFICATE_EXPIRATION__

Installation
------------

1. Verify the files with SHA256SUMS.txt.
2. Open __CERTIFICATE_FILE__.
3. Select Install Certificate, choose Local Machine, approve the
   administrator prompt, and place the certificate in Trusted People.
4. Open __PACKAGE_FILE__ and select Install.

PowerShell equivalent (run PowerShell as administrator):

  Import-Certificate `
    -FilePath .\__CERTIFICATE_FILE__ `
    -CertStoreLocation Cert:\LocalMachine\TrustedPeople

  Add-AppxPackage .\__PACKAGE_FILE__

The package registers winterm.exe. It does not register or replace wt.exe.

Remove self-signed trust
------------------------

After uninstalling winTerm, run PowerShell as administrator:

  $certificate = [Security.Cryptography.X509Certificates.X509Certificate2]::new(
    (Resolve-Path .\__CERTIFICATE_FILE__))
  Remove-Item "Cert:\LocalMachine\TrustedPeople\$($certificate.Thumbprint)"
'@
    $instructions = $instructions.
        Replace('__APPLICATION_VERSION__', $applicationVersion).
        Replace('__PACKAGE_DESCRIPTION__', $packageDescription.Trim()).
        Replace('__CERTIFICATE_FILE__', $certificateName).
        Replace('__PACKAGE_FILE__', $signedPackageName).
        Replace('__COMMIT_SHA__', $commitSha).
        Replace('__WORKING_TREE_DIRTY__', $workingTreeDirty.ToString().ToLowerInvariant()).
        Replace('__CERTIFICATE_THUMBPRINT__', $certificateThumbprint).
        Replace(
            '__CERTIFICATE_EXPIRATION__',
            $certificate.NotAfter.ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))
    [IO.File]::WriteAllText(
        (Join-Path $outputRoot 'INSTALL.txt'),
        $instructions + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))

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

    $completed = $true
    Write-Host "Created self-signed winTerm package: $signedPackagePath" -ForegroundColor Green
    Write-Host "Public signing certificate: $certificatePath"
    Write-Host "Checksums: $(Join-Path $outputRoot 'SHA256SUMS.txt')"
    Write-Warning 'Trust the included public certificate only after verifying the package source and SHA-256 hashes.'
}
catch
{
    Write-Error "Local development build failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    if (-not [string]::IsNullOrWhiteSpace($certificateThumbprint))
    {
        $certificateStorePath = "Cert:\CurrentUser\My\$certificateThumbprint"
        if (Test-Path -LiteralPath $certificateStorePath)
        {
            Remove-Item -LiteralPath $certificateStorePath -Force
        }
    }
    if ($null -ne $temporaryDirectory -and
        (Test-Path -LiteralPath $temporaryDirectory))
    {
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
    if ($null -ne $gitExcludesPath -and
        (Test-Path -LiteralPath $gitExcludesPath))
    {
        Remove-Item -LiteralPath $gitExcludesPath -Force
    }
    if (-not $completed -and $createdOutputDirectory -and
        (Test-Path -LiteralPath $outputRoot))
    {
        Remove-Item -LiteralPath $outputRoot -Recurse -Force
    }
}
