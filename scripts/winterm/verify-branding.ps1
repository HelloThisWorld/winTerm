# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [string]$PackageOutput,

    [Parameter()]
    [ValidatePattern('^CN=')]
    [string]$ExpectedPublisher = 'CN=winTerm Development'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$failures = New-Object System.Collections.Generic.List[string]
$temporaryDirectory = $null

function Test-Requirement
{
    param(
        [Parameter(Mandatory)]
        [bool]$Condition,

        [Parameter(Mandatory)]
        [string]$Message
    )

    if ($Condition)
    {
        Write-Host "PASS: $Message" -ForegroundColor Green
    }
    else
    {
        $script:failures.Add($Message)
        Write-Host "FAIL: $Message" -ForegroundColor Red
    }
}

function Get-ManifestData
{
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    [xml]$manifest = Get-Content -LiteralPath $Path -Raw
    $namespaceManager = New-Object System.Xml.XmlNamespaceManager($manifest.NameTable)
    $namespaceManager.AddNamespace('f', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
    $namespaceManager.AddNamespace('uap', 'http://schemas.microsoft.com/appx/manifest/uap/windows10')
    $namespaceManager.AddNamespace('uap3', 'http://schemas.microsoft.com/appx/manifest/uap/windows10/3')
    $namespaceManager.AddNamespace('desktop', 'http://schemas.microsoft.com/appx/manifest/desktop/windows10')

    return @{
        Document = $manifest
        Namespaces = $namespaceManager
    }
}

function Test-Manifest
{
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $manifestData = Get-ManifestData -Path $Path
    $manifest = $manifestData.Document
    $namespaces = $manifestData.Namespaces
    $identity = $manifest.SelectSingleNode('/f:Package/f:Identity', $namespaces)
    $properties = $manifest.SelectSingleNode('/f:Package/f:Properties', $namespaces)
    $application = $manifest.SelectSingleNode('/f:Package/f:Applications/f:Application', $namespaces)
    $visualElements = $manifest.SelectSingleNode('//uap:VisualElements', $namespaces)
    $aliases = @($manifest.SelectNodes('//uap3:AppExecutionAlias/desktop:ExecutionAlias', $namespaces) | ForEach-Object { $_.Alias })

    Test-Requirement -Condition ($null -ne $identity -and $identity.Name -eq 'HelloThisWorld.winTerm') -Message "$Path uses package identity HelloThisWorld.winTerm"
    Test-Requirement -Condition ($null -ne $identity -and $identity.Name -notmatch '^Microsoft\.') -Message "$Path does not use a Microsoft package name"
    Test-Requirement -Condition ($null -ne $identity -and $identity.Publisher -ceq $ExpectedPublisher) -Message "$Path uses the expected non-Microsoft publisher"
    Test-Requirement -Condition ($null -ne $identity -and $identity.Version -eq '1.0.2.0') -Message "$Path uses package version 1.0.2.0"
    Test-Requirement -Condition ($null -ne $properties -and $properties.DisplayName -eq 'winTerm') -Message "$Path package display name is winTerm"
    Test-Requirement -Condition ($null -ne $application -and $application.Id -eq 'winTerm') -Message "$Path application ID is winTerm"
    Test-Requirement -Condition ($null -ne $visualElements -and $visualElements.DisplayName -eq 'winTerm') -Message "$Path application display name is winTerm"
    Test-Requirement -Condition ($null -ne $visualElements -and $visualElements.Description -match '^Independent open-source terminal') -Message "$Path description states independent open-source status"
    Test-Requirement -Condition ($null -ne $visualElements -and $visualElements.Description -notmatch '(?i)official Microsoft|endorsed by Microsoft') -Message "$Path description does not imply Microsoft endorsement"
    Test-Requirement -Condition ($aliases -contains 'winterm.exe') -Message "$Path registers winterm.exe"
    Test-Requirement -Condition ($aliases -notcontains 'wt.exe') -Message "$Path does not register wt.exe"
}

function Test-BuildOutput
{
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $resolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $inspectionRoot = $resolvedPath

    if (Test-Path -LiteralPath $resolvedPath -PathType Leaf)
    {
        if ([System.IO.Path]::GetExtension($resolvedPath) -ne '.msix')
        {
            throw "Package output '$resolvedPath' must be an MSIX file or an unpacked package directory."
        }

        $makeAppx = Get-Command makeappx.exe -ErrorAction SilentlyContinue
        if ($null -eq $makeAppx)
        {
            throw 'makeappx.exe is required to inspect an MSIX package.'
        }

        $script:temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("winterm-branding-{0}" -f [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $script:temporaryDirectory | Out-Null
        & $makeAppx.Source unpack /p $resolvedPath /d $script:temporaryDirectory /o
        if ($LASTEXITCODE -ne 0)
        {
            throw "makeappx.exe failed with exit code $LASTEXITCODE."
        }
        $inspectionRoot = $script:temporaryDirectory
    }

    $builtManifest = Get-ChildItem -LiteralPath $inspectionRoot -Recurse -File -Filter 'AppxManifest.xml' | Select-Object -First 1
    Test-Requirement -Condition ($null -ne $builtManifest) -Message 'Build output contains AppxManifest.xml'
    if ($null -ne $builtManifest)
    {
        Test-Manifest -Path $builtManifest.FullName
    }

    $launcher = Get-ChildItem -LiteralPath $inspectionRoot -Recurse -File -Filter 'winterm.exe' | Select-Object -First 1
    $forbiddenLauncher = Get-ChildItem -LiteralPath $inspectionRoot -Recurse -File -Filter 'wt.exe' | Select-Object -First 1
    $host = Get-ChildItem -LiteralPath $inspectionRoot -Recurse -File -Filter 'WindowsTerminal.exe' | Select-Object -First 1

    Test-Requirement -Condition ($null -ne $launcher) -Message 'Build output contains winterm.exe'
    Test-Requirement -Condition ($null -eq $forbiddenLauncher) -Message 'Build output does not contain wt.exe'
    Test-Requirement -Condition ($null -ne $host) -Message 'Build output contains the internal terminal host executable'
    if ($null -ne $host)
    {
        Test-Requirement -Condition ($host.VersionInfo.FileDescription -eq 'winTerm Terminal Host') -Message 'Terminal host file description is winTerm Terminal Host'
        Test-Requirement -Condition ($host.VersionInfo.ProductName -eq 'winTerm') -Message 'Terminal host product name is winTerm'
    }
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifestPath = Join-Path $repositoryRoot 'src\cascadia\CascadiaPackage\Package-winTerm.appxmanifest'

try
{
    Test-Manifest -Path $manifestPath

    $requiredFiles = @(
        'LICENSE',
        'NOTICE.md',
        'THIRD_PARTY_NOTICES.md',
        'src\cascadia\CascadiaPackage\NOTICE.html',
        'assets\winterm\licenses\open-source-licenses.html',
        'assets\winterm\icons\winterm.svg',
        'assets\winterm\icons\winterm.ico',
        'res\terminal\images-WinTerm\StoreLogo.png',
        'res\terminal\images-WinTerm\Square44x44Logo.png',
        'res\terminal\images-WinTerm\Square150x150Logo.png',
        'res\terminal\images-WinTerm\SmallTile.png',
        'res\terminal\images-WinTerm\Wide310x150Logo.png',
        'res\terminal\images-WinTerm\LargeTile.png',
        'res\terminal\images-WinTerm\terminal.ico'
    )
    foreach ($relativePath in $requiredFiles)
    {
        Test-Requirement -Condition (Test-Path -LiteralPath (Join-Path $repositoryRoot $relativePath)) -Message "Required file exists: $relativePath"
    }

    $packageProject = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\cascadia\CascadiaPackage\CascadiaPackage.wapproj') -Raw
    $brandingTargets = Get-Content -LiteralPath (Join-Path $repositoryRoot 'build\rules\Branding.targets') -Raw
    $settingsPaths = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\cascadia\TerminalSettingsModel\FileUtils.cpp') -Raw
    $terminalProject = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\cascadia\WindowsTerminal\WindowsTerminal.vcxproj') -Raw
    $launcherProject = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\cascadia\wt\wt.vcxproj') -Raw
    $resourceItems = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\cascadia\CascadiaResources.build.items') -Raw
    $executablePathHelper = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\cascadia\WinRTUtils\inc\WtExeUtils.h') -Raw
    $aboutDialog = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\cascadia\TerminalApp\AboutDialog.cpp') -Raw

    Test-Requirement -Condition ($packageProject.Contains('Package-winTerm.appxmanifest') -and $packageProject.Contains('<OCExecutionAliasName Condition="''$(WindowsTerminalBranding)''==''WinTerm''">winterm</OCExecutionAliasName>')) -Message 'Package project selects the winTerm manifest and launcher alias'
    Test-Requirement -Condition ($brandingTargets.Contains('WT_BRANDING_WINTERM')) -Message 'Dedicated winTerm compile-time branding token exists'
    Test-Requirement -Condition ($settingsPaths.Contains('L"winTerm\\"') -and $settingsPaths.Contains('return GetBaseSettingsPath();')) -Message 'Unpackaged settings and release-migration paths are isolated'
    Test-Requirement -Condition ($terminalProject.Contains('winTerm Terminal Host')) -Message 'Terminal host source metadata uses winTerm'
    Test-Requirement -Condition ($launcherProject.Contains('winTerm Launcher')) -Message 'Launcher source metadata uses winTerm'
    Test-Requirement -Condition ($resourceItems.Contains('$(OpenConsoleDir)NOTICE.md') -and $resourceItems.Contains('<Link>NOTICE.md</Link>') -and $resourceItems.Contains('$(OpenConsoleDir)LICENSE') -and $resourceItems.Contains('<Link>LICENSE</Link>')) -Message 'winTerm package maps complete offline notices and the repository license'
    Test-Requirement -Condition ($resourceItems.Contains('AppearanceAssets\fonts\bundled\%(FileName)%(Extension)') -and $resourceItems.Contains('''$(WindowsTerminalBranding)''==''WinTerm''')) -Message 'winTerm package maps bundled fonts to an app-private appearance path'
    Test-Requirement -Condition ($executablePathHelper -match '(?s)#if defined\(WT_BRANDING_WINTERM\)\s+WinTermExe;' -and $executablePathHelper.Contains('return std::wstring{ WinTermExe };')) -Message 'New-window and jump-list launch paths use winterm.exe for winTerm branding'
    Test-Requirement -Condition ($aboutDialog.Contains('AppearanceAssets\\licenses\\open-source-licenses.html')) -Message 'winTerm About dialog opens its bundled offline license index'

    if (-not [string]::IsNullOrWhiteSpace($PackageOutput))
    {
        Test-BuildOutput -Path $PackageOutput
    }
    else
    {
        Write-Host 'SKIP: Build-output metadata checks (no -PackageOutput supplied).' -ForegroundColor Yellow
    }

    if ($failures.Count -gt 0)
    {
        throw ("Branding verification failed:`n - " + ($failures -join "`n - "))
    }

    Write-Host 'winTerm branding verification passed.' -ForegroundColor Green
}
catch
{
    Write-Error $_.Exception.Message
    exit 1
}
finally
{
    if ($null -ne $temporaryDirectory -and (Test-Path -LiteralPath $temporaryDirectory))
    {
        Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
    }
}
