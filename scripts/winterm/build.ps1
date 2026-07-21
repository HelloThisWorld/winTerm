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

function Set-SingleProcessPath
{
    param(
        [Parameter(Mandatory)]
        [string]$Value
    )

    # PowerShell 7 can preserve both PATH and Path from a sandbox host. Its Env:
    # provider can address the exact uppercase key, whereas the .NET setter uses
    # a case-insensitive lookup and may leave the duplicate behind.
    Remove-Item -LiteralPath 'Env:PATH' -ErrorAction SilentlyContinue
    $env:Path = $Value
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$originalLocation = Get-Location
$originalProcessPath = [Environment]::GetEnvironmentVariable('Path', [EnvironmentVariableTarget]::Process)
$originalGitConfigCount = $env:GIT_CONFIG_COUNT
$originalGitConfigKey = $env:GIT_CONFIG_KEY_0
$originalGitConfigValue = $env:GIT_CONFIG_VALUE_0

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
    $testProjects = [ordered]@{
        'Settings Model unit test project' = Join-Path $repositoryRoot 'src\cascadia\UnitTests_SettingsModel\SettingsModel.UnitTests.vcxproj'
        'Terminal App unit test project'   = Join-Path $repositoryRoot 'src\cascadia\ut_app\TerminalApp.UnitTests.vcxproj'
        'Control unit test project'        = Join-Path $repositoryRoot 'src\cascadia\UnitTests_Control\Control.UnitTests.vcxproj'
    }
    foreach ($testProject in $testProjects.GetEnumerator())
    {
        Assert-PathExists -Path $testProject.Value -Description $testProject.Key
    }
    Assert-PathExists -Path $visualStudioInstaller -Description 'Visual Studio Installer discovery tool'
    Assert-PathExists -Path $windowsSdk -Description 'Windows SDK 10.0.22621.0'

    # Some sandboxed Windows hosts expose both PATH and Path. MSBuild ToolTask
    # rejects that duplicate environment block when it starts rc.exe or midl.exe.
    Set-SingleProcessPath -Value $originalProcessPath
    $env:GIT_CONFIG_COUNT = '1'
    $env:GIT_CONFIG_KEY_0 = 'safe.directory'
    $env:GIT_CONFIG_VALUE_0 = $repositoryRoot.Replace('\', '/')

    Set-Location $repositoryRoot
    Import-Module (Join-Path $repositoryRoot 'tools\OpenConsole.psm1') -Force
    Set-MsBuildDevEnvironment
    Set-SingleProcessPath -Value ([Environment]::GetEnvironmentVariable('Path', [EnvironmentVariableTarget]::Process))

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
        '/verbosity:minimal',
        # The packaging dependency graph contains WinMD and shared-PDB edges that
        # are not safe under project-level parallelism in this upstream revision.
        # Keep compiler output visible while serializing projects for reliability.
        '/m:1'
    )

    if ($GeneratePackage)
    {
        $msbuildArguments += '/p:GenerateAppxPackageOnBuild=true'
        $msbuildArguments += '/p:AppxPackageSigningEnabled=false'
    }

    Write-Host "Building winTerm package dependency graph ($Configuration, $Platform)..."
    Invoke-OpenConsoleBuild @msbuildArguments '/t:Terminal\CascadiaPackage'
    if ($LASTEXITCODE -ne 0)
    {
        throw "MSBuild failed with exit code $LASTEXITCODE."
    }

    if ($IncludeTests)
    {
        # Test projects build directly rather than through the solution, so supply
        # SolutionDir yourself; the repository props derive imports and output
        # paths from it and expect the trailing slash MSBuild normally provides.
        $solutionDirArgument = "/p:SolutionDir=$repositoryRoot\"
        foreach ($testProject in $testProjects.GetEnumerator())
        {
            Write-Host "Building winTerm $($testProject.Key) ($Configuration, $Platform)..."
            msbuild.exe $testProject.Value /restore @msbuildArguments $solutionDirArgument
            if ($LASTEXITCODE -ne 0)
            {
                throw "$($testProject.Key) build failed with exit code $LASTEXITCODE."
            }
        }
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
    Set-SingleProcessPath -Value $originalProcessPath
    $env:GIT_CONFIG_COUNT = $originalGitConfigCount
    $env:GIT_CONFIG_KEY_0 = $originalGitConfigKey
    $env:GIT_CONFIG_VALUE_0 = $originalGitConfigValue
}
