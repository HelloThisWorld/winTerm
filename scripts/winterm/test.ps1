# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet('Smoke', 'Relevant', 'Full')]
    [string]$Suite = 'Smoke',

    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [Parameter()]
    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [switch]$Build
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Test-PowerShellSyntax
{
    param(
        [Parameter(Mandatory)]
        [string]$Directory
    )

    foreach ($scriptFile in Get-ChildItem -LiteralPath $Directory -File -Filter '*.ps1')
    {
        $tokens = $null
        $parseErrors = $null
        [System.Management.Automation.Language.Parser]::ParseFile($scriptFile.FullName, [ref]$tokens, [ref]$parseErrors) | Out-Null
        if ($parseErrors.Count -gt 0)
        {
            $details = $parseErrors | ForEach-Object { "line $($_.Extent.StartLineNumber): $($_.Message)" }
            throw "PowerShell syntax failed for '$($scriptFile.FullName)': $($details -join '; ')"
        }
        Write-Host "PASS: PowerShell syntax: $($scriptFile.Name)" -ForegroundColor Green
    }
}

function Test-ProfileFoundations
{
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot
    )

    $userDefaults = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalSettingsModel\userDefaults.json') -Raw
    $settingsSerialization = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalSettingsModel\CascadiaSettingsSerialization.cpp') -Raw
    $powerShellGenerator = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalSettingsModel\PowershellCoreProfileGenerator.cpp') -Raw
    $wslGenerator = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalSettingsModel\WslDistroGenerator.cpp') -Raw

    if (-not ($userDefaults.Contains('"name": "Windows PowerShell"') -and $userDefaults.Contains('WindowsPowerShell\\v1.0\\powershell.exe')))
    {
        throw 'Windows PowerShell 5.1 profile foundation is missing.'
    }
    Write-Host 'PASS: Windows PowerShell 5.1 profile foundation' -ForegroundColor Green

    if (-not ($userDefaults.Contains('%SystemRoot%\\System32\\cmd.exe') -and $settingsSerialization.Contains('CommandPromptDisplayName')))
    {
        throw 'Command Prompt profile foundation is missing.'
    }
    Write-Host 'PASS: Command Prompt profile foundation' -ForegroundColor Green

    if (-not ($powerShellGenerator.Contains('POWERSHELL_PREFERRED_PROFILE_NAME{ L"PowerShell" }') -and $settingsSerialization.Contains('preferredPowershellProfile')))
    {
        throw 'PowerShell 7 discovery or default preference is missing.'
    }
    Write-Host 'PASS: PowerShell 7 discovery and first-launch preference' -ForegroundColor Green

    if (-not ($wslGenerator.Contains('openWslRegKey()') -and $wslGenerator.Contains('if (wslRootKey)')))
    {
        throw 'WSL dynamic discovery or missing-WSL guard is missing.'
    }
    Write-Host 'PASS: WSL dynamic discovery and missing-WSL guard' -ForegroundColor Green
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

try
{
    Write-Host "Running winTerm $Suite tests ($Configuration, $Platform)..."
    Test-PowerShellSyntax -Directory $PSScriptRoot

    & (Join-Path $PSScriptRoot 'verify-branding.ps1')
    if (-not $?)
    {
        throw 'Branding verification failed.'
    }

    Test-ProfileFoundations -RepositoryRoot $repositoryRoot

    if ($Suite -eq 'Smoke')
    {
        Write-Host 'SKIP: Compiled unit tests are not part of the Smoke suite.' -ForegroundColor Yellow
        Write-Host 'winTerm Smoke tests passed.' -ForegroundColor Green
        return
    }

    if ($PSVersionTable.PSVersion.Major -lt 7)
    {
        throw 'PowerShell 7 or later is required for compiled upstream tests.'
    }

    if ($Build)
    {
        & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform
        if (-not $?)
        {
            throw 'Build failed.'
        }
    }

    $binaryDirectory = Join-Path $repositoryRoot "bin\$Platform\$Configuration"
    if (-not (Test-Path -LiteralPath $binaryDirectory))
    {
        throw "Compiled test output '$binaryDirectory' was not found. Run build.ps1 first or pass -Build."
    }

    Set-Location $repositoryRoot
    Import-Module (Join-Path $repositoryRoot 'tools\OpenConsole.psm1') -Force
    Set-MsBuildDevEnvironment

    if ($Suite -eq 'Relevant')
    {
        foreach ($testName in @('unitSettingsModel', 'terminalApp', 'unitControl'))
        {
            Write-Host "Running upstream test group: $testName"
            Invoke-OpenConsoleTests -Test $testName -Platform $Platform -Configuration $Configuration
            if (-not $?)
            {
                throw "Upstream test group '$testName' failed."
            }
        }
    }
    else
    {
        Invoke-OpenConsoleTests -AllTests -Platform $Platform -Configuration $Configuration
        if (-not $?)
        {
            throw 'Full upstream tests failed.'
        }
    }

    Write-Host "winTerm $Suite tests passed." -ForegroundColor Green
}
catch
{
    Write-Error "winTerm tests failed: $($_.Exception.Message)"
    exit 1
}
