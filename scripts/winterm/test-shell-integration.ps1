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
            Assert-Condition -Condition ($nativeOutput -match '(?i)Get-ChildItem') -Message 'The native which command did not report Get-ChildItem.'
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

function Test-PowerShellPromptExitCodes
{
    param(
        [Parameter(Mandatory)]
        [string]$ModulePath
    )

    $originalSessionId = $env:WINTERM_SESSION_ID
    $env:WINTERM_SESSION_ID = 'test-shell-integration'
    $temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ('winterm-prompt-' + [guid]::NewGuid().ToString('N'))
    $escape = [char]27

    try
    {
        Import-Module $ModulePath -Force
        New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null
        $enabled = Enable-WinTermShellIntegration -Force
        Assert-Condition -Condition $enabled -Message 'Shell integration could not be enabled for the prompt exit code test.'

        # The wrapper installed as the global prompt function must observe the
        # $? produced by the statement directly before each prompt call, the
        # same way the console host invokes it after a finished command line.
        $firstPrompt = prompt
        Assert-Condition -Condition $firstPrompt.Contains("$escape]133;A") -Message 'The first prompt did not emit a prompt-start mark.'
        Assert-Condition -Condition (-not $firstPrompt.Contains("$escape]133;D")) -Message 'The first prompt emitted a finished mark before any command ran.'

        $null = Get-Command -Name prompt
        $successPrompt = prompt
        Assert-Condition -Condition $successPrompt.Contains("$escape]133;D;0") -Message 'A successful command was not reported as exit code 0.'

        & $env:ComSpec /c exit 0
        Get-Item -LiteralPath (Join-Path $temporaryDirectory 'missing.txt') -ErrorAction SilentlyContinue
        $cmdletFailurePrompt = prompt
        Assert-Condition -Condition $cmdletFailurePrompt.Contains("$escape]133;D;1") -Message 'A failed cmdlet was not reported as exit code 1.'

        & $env:ComSpec /c exit 5
        $nativeFailurePrompt = prompt
        Assert-Condition -Condition $nativeFailurePrompt.Contains("$escape]133;D;5") -Message 'A native command exit code was not propagated to the finished mark.'

        $null = Get-Command -Name prompt
        $recoveredPrompt = prompt
        Assert-Condition -Condition $recoveredPrompt.Contains("$escape]133;D;0") -Message 'A success after a failure was not reported as exit code 0.'
    }
    finally
    {
        Disable-WinTermShellIntegration | Out-Null
        Remove-Module -Name winTerm.Shell -Force -ErrorAction SilentlyContinue
        if (Test-Path -LiteralPath $temporaryDirectory)
        {
            Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
        }
        $env:WINTERM_SESSION_ID = $originalSessionId
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
        $outputText = $output -join [Environment]::NewLine
        Assert-Condition -Condition ($outputText -match 'll=dir /a \$\*') -Message 'CMD initialization did not register ll.'

        & cmd.exe /d /c 'where touch.exe >nul 2>nul'
        $nativeTouchExitCode = $LASTEXITCODE
        if ($nativeTouchExitCode -eq 0)
        {
            Assert-Condition -Condition ($outputText -notmatch '(?m)^touch=') -Message 'CMD initialization overrode a native touch executable.'
        }
        else
        {
            Assert-Condition -Condition ($outputText -match 'touch="C:\\Program Files\\winTerm\\ShellAssets\\winterm-shim.exe" touch \$\*') -Message 'CMD touch did not preserve a quoted helper path.'
        }

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
    Test-PowerShellPromptExitCodes -ModulePath $modulePath
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
