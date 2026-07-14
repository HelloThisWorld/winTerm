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
    $originalPath = $env:PATH
    $env:WINTERM_SESSION_ID = 'test-shell-integration'
    $env:PATH = Join-Path $env:SystemRoot 'System32'
    $temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ('winterm-shell-' + [guid]::NewGuid().ToString('N'))

    try
    {
        Import-Module $ModulePath -Force
        Set-WinTermCompatibilityMode -Mode Safe | Out-Null
        New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null

        $createdFile = Join-Path $temporaryDirectory 'created.txt'
        $touchCommand = Get-Command -Name touch -ErrorAction Stop
        if ($touchCommand.ModuleName -eq 'winTerm.Shell')
        {
            touch $createdFile
            Assert-Condition -Condition (Test-Path -LiteralPath $createdFile -PathType Leaf) -Message 'touch did not create a file.'

            Set-Content -LiteralPath $createdFile -Value 'preserved'
            touch $createdFile
            Assert-Condition -Condition ((Get-Content -LiteralPath $createdFile -Raw).Trim() -eq 'preserved') -Message 'touch truncated an existing file.'
        }
        else
        {
            Write-Host "SKIP: touch resolves to the existing $($touchCommand.CommandType) command."
        }

        $listing = @(ll $temporaryDirectory)
        Assert-Condition -Condition ($listing.Count -gt 0) -Message 'll did not list the requested directory.'
        $llCommand = Get-Command -Name ll -ErrorAction Stop
        Assert-Condition -Condition ($llCommand.ModuleName -eq 'winTerm.Shell') -Message 'll was not provided by winTerm Shell.'

        $whichCommand = Get-Command -Name which -ErrorAction Stop
        $found = @(which Get-ChildItem)
        Assert-Condition -Condition ($found.Count -gt 0) -Message 'which did not find Get-ChildItem.'
        if ($whichCommand.ModuleName -eq 'winTerm.Shell')
        {
            Assert-Condition -Condition ($found[0].Name -eq 'Get-ChildItem') -Message 'winTerm which did not return the requested command.'
        }
        else
        {
            $nativeOutput = ($found | ForEach-Object { [string]$_ }) -join [Environment]::NewLine
            Assert-Condition -Condition ($nativeOutput -match '(?i)get-childitem') -Message 'The native which command did not report Get-ChildItem.'
        }

        Set-WinTermCompatibilityMode -Mode Off | Out-Null
        $disabled = $false
        try
        {
            ll $temporaryDirectory -ErrorAction Stop
        }
        catch
        {
            $disabled = $true
        }
        Assert-Condition -Condition $disabled -Message 'Compatibility mode Off did not disable ll.'

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
        $env:PATH = $originalPath
    }
}

function Test-PowerShellNativeCommandPrecedence
{
    param(
        [Parameter(Mandatory)]
        [string]$ModulePath
    )

    $nativeTouch = Get-Command -Name touch -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $nativeTouch)
    {
        Write-Host 'SKIP: No native touch application is installed.' -ForegroundColor Yellow
        return
    }

    try
    {
        Import-Module $ModulePath -Force
        $resolvedTouch = Get-Command -Name touch -ErrorAction Stop
        Assert-Condition -Condition ($resolvedTouch.CommandType -eq 'Application') -Message 'winTerm Shell overrode a native touch application.'
    }
    finally
    {
        Remove-Module -Name winTerm.Shell -Force -ErrorAction SilentlyContinue
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
    Test-PowerShellNativeCommandPrecedence -ModulePath $modulePath
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
