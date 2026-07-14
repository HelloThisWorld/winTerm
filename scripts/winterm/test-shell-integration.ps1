# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet('PowerShell7', 'WindowsPowerShell', 'CMD', 'All')]
    [string]$Shell = 'All'
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
}

function Test-PowerShellModule
{
    param(
        [Parameter(Mandatory)]
        [string]$ModulePath
    )

    $originalSessionId = $env:WINTERM_SESSION_ID
    $env:WINTERM_SESSION_ID = 'test-shell-integration'
    $temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ('winterm-shell-' + [guid]::NewGuid().ToString('N'))

    try
    {
        Import-Module $ModulePath -Force
        Set-WinTermCompatibilityMode -Mode Safe | Out-Null
        New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null

        touch (Join-Path $temporaryDirectory 'created.txt')
        $createdFile = Join-Path $temporaryDirectory 'created.txt'
        Assert-Condition -Condition (Test-Path -LiteralPath $createdFile -PathType Leaf) -Message 'touch did not create a file.'

        Set-Content -LiteralPath $createdFile -Value 'preserved'
        touch $createdFile
        Assert-Condition -Condition ((Get-Content -LiteralPath $createdFile -Raw).Trim() -eq 'preserved') -Message 'touch truncated an existing file.'

        $listing = @(ll $temporaryDirectory)
        Assert-Condition -Condition ($listing.Count -gt 0) -Message 'll did not list the requested directory.'

        $found = which powershell
        Assert-Condition -Condition ($found.Name -eq 'powershell') -Message 'which did not find powershell.'

        Set-WinTermCompatibilityMode -Mode Off | Out-Null
        $disabled = $false
        try
        {
            touch (Join-Path $temporaryDirectory 'disabled.txt') -ErrorAction Stop
        }
        catch
        {
            $disabled = $true
        }
        Assert-Condition -Condition $disabled -Message 'Compatibility mode Off did not disable touch.'

        Set-WinTermCompatibilityMode -Mode Safe | Out-Null
        $diagnostics = Get-WinTermShellDiagnostics
        Assert-Condition -Condition ($diagnostics.ProtocolVersion -eq 1) -Message 'Shell diagnostics did not report protocol version 1.'
    }
    finally
    {
        Remove-Module -Name winTerm.Shell -Force -ErrorAction SilentlyContinue
        if (Test-Path -LiteralPath $temporaryDirectory)
        {
            Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
        }
        $env:WINTERM_SESSION_ID = $originalSessionId
    }
}

function Test-CmdInitialization
{
    param(
        [Parameter(Mandatory)]
        [string]$InitScript
    )

    $doskeyScript = Join-Path (Split-Path -Parent $InitScript) 'winterm-doskey.cmd'
    $originalShim = $env:WINTERM_SHIM
    $originalMode = $env:WINTERM_COMPATIBILITY_MODE
    $env:WINTERM_SHIM = 'C:\Program Files\winTerm\ShellAssets\winterm-shim.exe'
    $env:WINTERM_COMPATIBILITY_MODE = 'Safe'

    try
    {
        $output = & cmd.exe /d /c ('call "{0}" & doskey /macros' -f $doskeyScript)
        Assert-Condition -Condition ($output -match 'll=dir /a \$\*') -Message 'CMD initialization did not register ll.'
        Assert-Condition -Condition ($output -match 'touch="C:\\Program Files\\winTerm\\ShellAssets\\winterm-shim.exe" touch \$\*') -Message 'CMD touch did not preserve a quoted helper path.'

        & cmd.exe /d /c ('call "{0}" & if not defined WINTERM_CMD_INITIALIZED exit 1 & if not defined WINTERM_INTEGRATION_VERSION exit 1' -f $InitScript)
        Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message 'CMD initialization did not set its process-local markers.'
    }
    finally
    {
        $env:WINTERM_SHIM = $originalShim
        $env:WINTERM_COMPATIBILITY_MODE = $originalMode
    }
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$modulePath = Join-Path $repositoryRoot 'shell\powershell\winTerm.Shell\winTerm.Shell.psd1'
$cmdInitPath = Join-Path $repositoryRoot 'shell\cmd\winterm-init.cmd'

if ($Shell -in @('WindowsPowerShell', 'All'))
{
    Test-PowerShellModule -ModulePath $modulePath
    Write-Host 'PASS: Windows PowerShell shell module' -ForegroundColor Green
}

if ($Shell -in @('PowerShell7', 'All'))
{
    $pwsh = Get-Command pwsh.exe -ErrorAction SilentlyContinue
    if ($null -eq $pwsh)
    {
        Write-Host 'SKIP: PowerShell 7 is not installed.' -ForegroundColor Yellow
    }
    else
    {
        & $pwsh.Source -NoLogo -NoProfile -File $PSCommandPath -Shell WindowsPowerShell
        if ($LASTEXITCODE -ne 0)
        {
            throw "PowerShell 7 shell module test failed with exit code $LASTEXITCODE."
        }
        Write-Host 'PASS: PowerShell 7 shell module' -ForegroundColor Green
    }
}

if ($Shell -in @('CMD', 'All'))
{
    Test-CmdInitialization -InitScript $cmdInitPath
    Write-Host 'PASS: Command Prompt initialization' -ForegroundColor Green
}
