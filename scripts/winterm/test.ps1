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

    foreach ($scriptFile in Get-ChildItem -LiteralPath $Directory -Recurse -File -Filter '*.ps1')
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

function Test-ShellExperienceFoundations
{
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot
    )

    $moduleManifest = Join-Path $RepositoryRoot 'shell\powershell\winTerm.Shell\winTerm.Shell.psd1'
    $moduleScript = Join-Path $RepositoryRoot 'shell\powershell\winTerm.Shell\winTerm.Shell.psm1'
    $cmdInit = Join-Path $RepositoryRoot 'shell\cmd\winterm-init.cmd'
    $protocol = Join-Path $RepositoryRoot 'src\winterm\Shell\Protocol\ShellIntegrationProtocol.cpp'

    foreach ($path in @($moduleManifest, $moduleScript, $cmdInit, $protocol))
    {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf))
        {
            throw "Shell Experience foundation is missing '$path'."
        }
    }

    $manifest = Import-PowerShellDataFile -LiteralPath $moduleManifest
    if ($manifest.ModuleVersion -ne '1.0.0' -or $manifest.PowerShellVersion -ne '5.1')
    {
        throw 'The winTerm PowerShell module manifest does not declare the supported version boundary.'
    }

    $moduleContent = Get-Content -LiteralPath $moduleScript -Raw
    $cmdContent = Get-Content -LiteralPath $cmdInit -Raw
    $protocolContent = Get-Content -LiteralPath $protocol -Raw
    if ($moduleContent -match 'ExecutionPolicy|Bypass' -or $cmdContent -match 'AutoRun' -or $protocolContent -notmatch 'MaximumShellProtocolPayloadLength')
    {
        throw 'Shell Experience safety boundaries are missing or contain a forbidden policy bypass.'
    }

    $shellScriptFiles = Get-ChildItem -LiteralPath (Split-Path -Parent $moduleScript) -Recurse -File -Include '*.ps1', '*.psm1'
    foreach ($shellScriptFile in $shellScriptFiles)
    {
        $tokens = $null
        $parseErrors = $null
        [System.Management.Automation.Language.Parser]::ParseFile($shellScriptFile.FullName, [ref]$tokens, [ref]$parseErrors) | Out-Null
        if ($parseErrors.Count -gt 0)
        {
            $details = $parseErrors | ForEach-Object { "line $($_.Extent.StartLineNumber): $($_.Message)" }
            throw "PowerShell syntax failed for '$($shellScriptFile.FullName)': $($details -join '; ')"
        }
    }

    & (Join-Path $PSScriptRoot 'package-shell-assets.ps1')
    if (-not $?)
    {
        throw 'Shell asset validation failed.'
    }

    & (Join-Path $PSScriptRoot 'test-paste-protection.ps1')
    if (-not $?)
    {
        throw 'Paste protection source validation failed.'
    }

    Write-Host 'PASS: Shell Experience source foundations' -ForegroundColor Green
}

function Test-WorkspaceFoundations
{
    $repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $settingsModel = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\cascadia\TerminalSettingsModel\MTSMSettings.h') -Raw
    if (-not ($settingsModel.Contains('WT_BRANDING_WINTERM') -and $settingsModel.Contains('FirstWindowPreference::PersistedLayout')))
    {
        throw 'winTerm does not default to the inherited safe persisted-layout startup path.'
    }

    foreach ($scriptName in @(
        'test-workspace-model.ps1',
        'test-workspace-restore.ps1',
        'test-workspace-recovery.ps1',
        'test-workspace-import.ps1'
    ))
    {
        & (Join-Path $PSScriptRoot $scriptName)
        if (-not $?)
        {
            throw "Workspace validation script '$scriptName' failed."
        }
    }
    Write-Host 'PASS: Workspace Restore source foundations' -ForegroundColor Green
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$originalLocation = Get-Location

try
{
    Write-Host "Running winTerm $Suite tests ($Configuration, $Platform)..."
    Test-PowerShellSyntax -Directory $PSScriptRoot

    & (Join-Path $PSScriptRoot 'verify-version.ps1')
    if (-not $?)
    {
        throw 'Version consistency validation failed.'
    }

    & (Join-Path $PSScriptRoot 'test-release-workflow.ps1')
    if (-not $?)
    {
        throw 'Release workflow validation failed.'
    }

    & (Join-Path $PSScriptRoot 'validate-assets.ps1')
    if (-not $?)
    {
        throw 'Appearance asset validation failed.'
    }

    & (Join-Path $PSScriptRoot 'generate-third-party-notices.ps1') -Check
    if (-not $?)
    {
        throw 'Third-party notice validation failed.'
    }

    & (Join-Path $PSScriptRoot 'verify-branding.ps1')
    if (-not $?)
    {
        throw 'Branding verification failed.'
    }

    Test-ProfileFoundations -RepositoryRoot $repositoryRoot
    Test-ShellExperienceFoundations -RepositoryRoot $repositoryRoot
    Test-WorkspaceFoundations

    & (Join-Path $PSScriptRoot 'test-diagnostics.ps1')
    if (-not $?)
    {
        throw 'Diagnostic redaction source validation failed.'
    }

    & (Join-Path $PSScriptRoot 'test-privacy.ps1')
    if (-not $?)
    {
        throw 'Privacy validation failed.'
    }

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
        & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform -IncludeTests
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

    $expectedTestBinaries = @(
        'UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll',
        'UnitTests_TerminalApp\Terminal.App.Unit.Tests.dll',
        'UnitTests_Control\Control.Unit.Tests.dll'
    )
    foreach ($relativeBinary in $expectedTestBinaries)
    {
        $testBinary = Join-Path $binaryDirectory $relativeBinary
        if (-not (Test-Path -LiteralPath $testBinary -PathType Leaf))
        {
            throw "Compiled test binary '$testBinary' was not found. Build the solution with -IncludeTests."
        }
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
finally
{
    Set-Location $originalLocation
}
