# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [Parameter()]
    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [switch]$GeneratePackage,

    [Parameter()]
    [switch]$IncludeTests
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-PathExists
{
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path))
    {
        throw "$Description was not found at '$Path'."
    }
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$originalLocation = Get-Location

try
{
    if ($PSVersionTable.PSVersion.Major -lt 7)
    {
        throw 'PowerShell 7 or later is required by the upstream Microsoft Terminal build module. Run this script with pwsh.exe.'
    }

    $visualStudioInstaller = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $windowsSdk = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Lib\10.0.22621.0'

    Assert-PathExists -Path (Join-Path $repositoryRoot 'OpenConsole.slnx') -Description 'OpenConsole solution'
    Assert-PathExists -Path (Join-Path $repositoryRoot 'tools\OpenConsole.psm1') -Description 'Upstream build module'
    Assert-PathExists -Path (Join-Path $repositoryRoot 'dep\nuget\nuget.exe') -Description 'Repository NuGet client'
    Assert-PathExists -Path (Join-Path $repositoryRoot 'src\cascadia\CascadiaPackage\CascadiaPackage.wapproj') -Description 'Cascadia package project'
    Assert-PathExists -Path $visualStudioInstaller -Description 'Visual Studio Installer discovery tool'
    Assert-PathExists -Path $windowsSdk -Description 'Windows SDK 10.0.22621.0'

    Set-Location $repositoryRoot
    Import-Module (Join-Path $repositoryRoot 'tools\OpenConsole.psm1') -Force
    Set-MsBuildDevEnvironment

    if (-not (Get-Command msbuild.exe -ErrorAction SilentlyContinue))
    {
        throw 'MSBuild was not found after initializing the Visual Studio development environment.'
    }

    $msbuildArguments = @(
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        '/p:WindowsTerminalBranding=WinTerm',
        '/p:AppxSymbolPackageEnabled=false',
        '/p:AppxBundle=Never',
        '/m'
    )

    if (-not $IncludeTests)
    {
        $msbuildArguments += '/t:Terminal\CascadiaPackage'
    }

    if ($GeneratePackage)
    {
        $msbuildArguments += '/p:GenerateAppxPackageOnBuild=true'
        $msbuildArguments += '/p:AppxPackageSigningEnabled=false'
    }

    $scope = if ($IncludeTests) { 'solution and tests' } else { 'package dependency graph' }
    Write-Host "Building winTerm $scope ($Configuration, $Platform)..."
    Invoke-OpenConsoleBuild @msbuildArguments
    if ($LASTEXITCODE -ne 0)
    {
        throw "MSBuild failed with exit code $LASTEXITCODE."
    }

    Write-Host "winTerm build completed successfully ($Configuration, $Platform)." -ForegroundColor Green
}
catch
{
    Write-Error "winTerm build failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    Set-Location $originalLocation
}
