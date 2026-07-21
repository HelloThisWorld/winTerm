# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Path,

    [Parameter()]
    [ValidateSet('Auto', 'Stage', 'Portable', 'Installer')]
    [string]$Type = 'Auto',

    [Parameter()]
    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [string]$Version
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$temporaryDirectory = $null
Add-Type -AssemblyName System.IO.Compression.FileSystem

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

function Test-RetiredBrandBytes
{
    param(
        [Parameter(Mandatory)]
        [System.IO.FileInfo]$File
    )

    $retiredName = -join ([char[]](0x4b, 0x61, 0x6e, 0x61, 0x6d, 0x65))
    $bytes = [IO.File]::ReadAllBytes($File.FullName)
    $ascii = [Text.Encoding]::ASCII.GetString($bytes)
    $unicode = [Text.Encoding]::Unicode.GetString($bytes)
    if ($ascii.IndexOf($retiredName, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or
        $unicode.IndexOf($retiredName, [StringComparison]::OrdinalIgnoreCase) -ge 0)
    {
        throw "Retired branding was found in '$($File.FullName)'."
    }
}

function Test-Stage
{
    param(
        [Parameter(Mandatory)]
        [string]$Root,

        [Parameter(Mandatory)]
        [bool]$Portable
    )

    $required = @(
        'winTerm.exe',
        'winterm.exe',
        'WindowsTerminal.exe',
        'OpenConsole.exe',
        'winterm-shim.exe',
        'Microsoft.UI.Xaml.dll',
        'resources.pri',
        'runtime\Microsoft.UI.Xaml.dll',
        'shell\powershell',
        'shell\cmd',
        'assets\themes',
        'assets\fonts',
        'assets\icons',
        'licenses\LICENSE',
        'THIRD_PARTY_NOTICES.md',
        'version.json'
    )
    foreach ($relativePath in $required)
    {
        Assert-Condition (Test-Path -LiteralPath (Join-Path $Root $relativePath)) "Payload contains $relativePath"
    }

    Assert-Condition ((Test-Path -LiteralPath (Join-Path $Root 'portable.marker')) -eq $Portable) 'Portable marker state matches the distribution type'
    if ($Portable)
    {
        Assert-Condition (Test-Path -LiteralPath (Join-Path $Root 'data') -PathType Container) 'Portable payload contains its data directory'
    }

    $forbiddenExtensions = @('.pdb', '.ilk', '.lib', '.exp', '.pfx', '.p12', '.key', '.cer', '.msix', '.msixbundle')
    $files = @(Get-ChildItem -LiteralPath $Root -Recurse -File -Force)
    foreach ($file in $files)
    {
        if ($file.Extension -in $forbiddenExtensions)
        {
            throw "Payload contains forbidden file '$($file.FullName)'."
        }
        if ($file.Name -in @('AppxManifest.xml', 'AppxSignature.p7x', 'AppxBlockMap.xml'))
        {
            throw "Payload contains package-identity file '$($file.FullName)'."
        }
        Test-RetiredBrandBytes -File $file
    }
    Write-Host "PASS: Inspected $($files.Count) payload file(s); no debug, signing-secret, MSIX, package-identity, or retired-brand content was found." -ForegroundColor Green

    $metadata = Get-Content -LiteralPath (Join-Path $Root 'version.json') -Raw | ConvertFrom-Json
    Assert-Condition ($metadata.publisher -ceq 'helloThisWorld') 'Payload metadata uses publisher helloThisWorld'
    Assert-Condition ($metadata.productId -ceq 'HelloThisWorld.winTerm') 'Payload metadata uses product ID HelloThisWorld.winTerm'
    Assert-Condition ($metadata.architecture -ceq $Platform.ToLowerInvariant()) 'Payload architecture matches the requested platform'

    foreach ($name in @('winTerm.exe', 'winterm.exe', 'WindowsTerminal.exe', 'winterm-shim.exe'))
    {
        $binary = Get-Item -LiteralPath (Join-Path $Root $name)
        Assert-Condition ($binary.VersionInfo.CompanyName -ceq 'helloThisWorld') "$name CompanyName is helloThisWorld"
        Assert-Condition ($binary.VersionInfo.ProductName -ceq 'winTerm') "$name ProductName is winTerm"
    }
}

try
{
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if ($Type -eq 'Auto')
    {
        $Type = if (Test-Path -LiteralPath $resolved -PathType Container) { 'Stage' } elseif ([IO.Path]::GetExtension($resolved) -eq '.zip') { 'Portable' } else { 'Installer' }
    }

    switch ($Type)
    {
        'Stage'
        {
            Assert-Condition (Test-Path -LiteralPath $resolved -PathType Container) 'Stage path is a directory'
            Test-Stage -Root $resolved -Portable $false
        }
        'Portable'
        {
            Assert-Condition ([IO.Path]::GetExtension($resolved) -eq '.zip') 'Portable artifact is a ZIP file'
            if (-not [string]::IsNullOrWhiteSpace($Version))
            {
                Assert-Condition ([IO.Path]::GetFileName($resolved) -ceq "winTerm-$Version-portable-$($Platform.ToLowerInvariant()).zip") 'Portable artifact name matches version and architecture'
            }
            $temporaryDirectory = Join-Path ([IO.Path]::GetTempPath()) ("winterm-portable-verify-{0}" -f [guid]::NewGuid().ToString('N'))
            [IO.Compression.ZipFile]::ExtractToDirectory($resolved, $temporaryDirectory)
            Test-Stage -Root $temporaryDirectory -Portable $true
        }
        'Installer'
        {
            $installer = Get-Item -LiteralPath $resolved
            Assert-Condition ($installer.Extension -eq '.exe') 'Installer artifact is an EXE'
            Assert-Condition ($installer.Length -gt 0) 'Installer artifact is not empty'
            if (-not [string]::IsNullOrWhiteSpace($Version))
            {
                Assert-Condition ($installer.Name -ceq "winTerm-$Version-setup-$($Platform.ToLowerInvariant()).exe") 'Installer artifact name matches version and architecture'
            }
            # Inno Setup reserves fixed-size version-resource fields and pads
            # them with spaces so post-build signing does not move file data.
            Assert-Condition ($installer.VersionInfo.CompanyName.TrimEnd() -ceq 'helloThisWorld') 'Installer CompanyName is helloThisWorld'
            Assert-Condition ($installer.VersionInfo.ProductName.TrimEnd() -ceq 'winTerm') 'Installer ProductName is winTerm'
            Test-RetiredBrandBytes -File $installer
            $signature = Get-AuthenticodeSignature -LiteralPath $installer.FullName
            Assert-Condition ($signature.Status -in @('Valid', 'NotSigned')) 'Installer signature state is valid or explicitly unsigned'
            Write-Host "Installer signing status: $($signature.Status)"
        }
    }

    Write-Host "winTerm $Type layout verification passed." -ForegroundColor Green
}
catch
{
    Write-Error "Release layout verification failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    if ($null -ne $temporaryDirectory -and (Test-Path -LiteralPath $temporaryDirectory))
    {
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
}
