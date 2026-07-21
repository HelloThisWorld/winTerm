# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$')]
    [string]$Version,

    [Parameter()]
    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [string]$StageDirectory,

    [Parameter()]
    [string]$OutputDirectory,

    [Parameter()]
    [string]$InnoCompiler,

    [Parameter()]
    [ValidatePattern('^[0-9A-Fa-f]{40}$')]
    [string]$SigningCertificateThumbprint,

    [Parameter()]
    [ValidatePattern('^https://')]
    [string]$TimestampUrl,

    [Parameter()]
    [switch]$SkipBuild,

    [Parameter()]
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Resolve-InnoCompiler
{
    if (-not [string]::IsNullOrWhiteSpace($InnoCompiler))
    {
        return (Resolve-Path -LiteralPath $InnoCompiler).Path
    }
    if (-not [string]::IsNullOrWhiteSpace($env:INNO_SETUP_COMPILER))
    {
        return (Resolve-Path -LiteralPath $env:INNO_SETUP_COMPILER).Path
    }
    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) { return $command.Source }
    foreach ($candidate in @(
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')))
    {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw 'Inno Setup 6 compiler was not found. Install the pinned CI version or pass -InnoCompiler.'
}

function Resolve-SignTool
{
    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $tool = Get-ChildItem -LiteralPath $sdkRoot -Recurse -File -Filter 'signtool.exe' |
        Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if ($null -eq $tool) { throw 'signtool.exe was not found in the Windows SDK.' }
    return $tool.FullName
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$platformLabel = $Platform.ToLowerInvariant()
$stageRoot = if ([string]::IsNullOrWhiteSpace($StageDirectory)) { Join-Path $repositoryRoot "artifacts\stage\winTerm-$platformLabel" } else { [IO.Path]::GetFullPath($StageDirectory) }
$outputRoot = if ([string]::IsNullOrWhiteSpace($OutputDirectory)) { Join-Path $repositoryRoot 'artifacts\release' } else { [IO.Path]::GetFullPath($OutputDirectory) }
$installerPath = Join-Path $outputRoot "winTerm-$Version-setup-$platformLabel.exe"
$numericVersion = (($Version -split '-', 2)[0] + '.0')

try
{
    & (Join-Path $PSScriptRoot 'verify-branding.ps1') -ExpectedPublisher 'CN=helloThisWorld'
    if (-not $?) { throw 'Branding verification failed.' }

    if (-not $SkipBuild -or -not (Test-Path -LiteralPath $stageRoot -PathType Container))
    {
        & (Join-Path $PSScriptRoot 'build-unpackaged.ps1') -Configuration Release -Platform $Platform -OutputDirectory $stageRoot -Force:$Force
        if (-not $?) { throw 'Unpackaged stage build failed.' }
    }
    & (Join-Path $PSScriptRoot 'verify-release-layout.ps1') -Path $stageRoot -Type Stage -Platform $Platform
    if (-not $?) { throw 'Source stage verification failed.' }

    New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
    if (Test-Path -LiteralPath $installerPath)
    {
        if (-not $Force) { throw "Installer artifact already exists: $installerPath" }
        Remove-Item -LiteralPath $installerPath -Force
    }

    $compiler = Resolve-InnoCompiler
    $scriptPath = Join-Path $repositoryRoot 'packaging\inno\winTerm.iss'
    Write-Host "Compiling winTerm installer with $compiler"
    & $compiler "/DStageDir=$stageRoot" "/DAppVersion=$Version" "/DNumericVersion=$numericVersion" "/DOutputDir=$outputRoot" "/DPlatformLabel=$platformLabel" $scriptPath
    if ($LASTEXITCODE -ne 0)
    {
        throw "Inno Setup failed with exit code $LASTEXITCODE."
    }
    if (-not (Test-Path -LiteralPath $installerPath -PathType Leaf))
    {
        throw "Inno Setup did not produce the expected installer '$installerPath'."
    }

    if (-not [string]::IsNullOrWhiteSpace($SigningCertificateThumbprint))
    {
        $signTool = Resolve-SignTool
        $arguments = @('sign', '/sha1', $SigningCertificateThumbprint, '/fd', 'SHA256')
        if (-not [string]::IsNullOrWhiteSpace($TimestampUrl))
        {
            $arguments += @('/tr', $TimestampUrl, '/td', 'SHA256')
        }
        $arguments += $installerPath
        & $signTool @arguments
        if ($LASTEXITCODE -ne 0) { throw "Authenticode signing failed with exit code $LASTEXITCODE." }
        & $signTool verify /pa /all /v $installerPath
        if ($LASTEXITCODE -ne 0) { throw "Authenticode verification failed with exit code $LASTEXITCODE." }
    }
    else
    {
        Write-Warning 'The installer is unsigned. Windows may show Unknown Publisher or a SmartScreen warning.'
    }

    & (Join-Path $PSScriptRoot 'verify-release-layout.ps1') -Path $installerPath -Type Installer -Platform $Platform -Version $Version
    if (-not $?) { throw 'Installer verification failed.' }
    Write-Host "Prepared winTerm installer: $installerPath" -ForegroundColor Green
}
catch
{
    Write-Error "Installer build failed: $($_.Exception.Message)"
    exit 1
}
