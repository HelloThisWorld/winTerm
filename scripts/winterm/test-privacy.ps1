# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

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
    Write-Host "PASS: $Message" -ForegroundColor Green
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

try
{
    $privacy = Get-Content -LiteralPath (Join-Path $repositoryRoot 'PRIVACY.md') -Raw
    foreach ($statement in @(
        'does not collect or upload command text',
        'terminal output',
        'clipboard content',
        'Workspace contents',
        'Crash-report upload is off by default',
        'Diagnostic bundles are created only when the user requests one'
    ))
    {
        Assert-Condition ($privacy.Contains($statement)) "Privacy policy covers '$statement'"
    }

    $aboutSource = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\cascadia\TerminalApp\AboutDialog.cpp') -Raw
    Assert-Condition ($aboutSource -match '(?s)#if defined\(WT_BRANDING_WINTERM\).*?co_return;.*?#else') 'winTerm update checks do not run without an explicit opt-in path'

    $providerSources = @(
        'src\cascadia\TerminalApp\init.cpp',
        'src\cascadia\TerminalConnection\init.cpp',
        'src\cascadia\TerminalControl\init.cpp',
        'src\cascadia\TerminalSettingsEditor\init.cpp',
        'src\cascadia\TerminalSettingsModel\init.cpp',
        'src\cascadia\WindowsTerminal\main.cpp'
    )
    foreach ($relativePath in $providerSources)
    {
        $providerSource = Get-Content -LiteralPath (Join-Path $repositoryRoot $relativePath) -Raw
        Assert-Condition ($providerSource -match '(?s)#if defined\(WT_BRANDING_WINTERM\).*?"winTerm\..*?#else.*?TraceLoggingOptionMicrosoftTelemetry\(\).*?#endif') "$relativePath separates the winTerm provider from Microsoft telemetry"
        Assert-Condition ($providerSource -match '(?s)#if !defined\(WT_BRANDING_WINTERM\).*?TraceLoggingRegister') "$relativePath does not register usage telemetry for winTerm"
    }

    $diagnosticSource = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Diagnostics\Redaction.cpp') -Raw
    foreach ($redaction in @('<user>', '<server>', '<share>', '<email>', '<redacted>', '<secret>', '<connection-endpoint>'))
    {
        Assert-Condition ($diagnosticSource.Contains($redaction)) "Diagnostic redaction contains $redaction"
    }

    $workspaceDiagnostics = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Workspaces\Diagnostics\WorkspaceDiagnostics.cpp') -Raw
    Assert-Condition (-not ($workspaceDiagnostics -match '(?i)terminalOutput|clipboard(Content|Text)|command(Line|Text)|workspaceJson')) 'Workspace diagnostics exclude commands, terminal output, clipboard content, and Workspace JSON'

    $dockingDiagnostics = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Docking\Diagnostics\DockingDiagnostics.cpp') -Raw
    Assert-Condition (-not ($dockingDiagnostics -match '(?i)terminalOutput|clipboard(Content|Text)|command(Line|Text)')) 'Docking diagnostics exclude commands, terminal output, and clipboard content'

    $loggingCandidates = @(Get-ChildItem -LiteralPath (Join-Path $repositoryRoot 'src\winterm') -Recurse -File -Include '*.cpp', '*.h')
    foreach ($file in $loggingCandidates)
    {
        $content = Get-Content -LiteralPath $file.FullName -Raw
        if ($content -match '(?i)(TraceLoggingWrite|OutputDebugString|WriteEvent).*(command(Line|Text)|clipboard(Content|Text)|terminalOutput)')
        {
            throw "Potential sensitive logging call found in '$($file.FullName)'."
        }
    }
    Write-Host 'PASS: no sensitive winTerm logging call was detected' -ForegroundColor Green

    & (Join-Path $PSScriptRoot 'test-diagnostics.ps1')
    if (-not $?)
    {
        throw 'Diagnostic redaction validation failed.'
    }

    Write-Host 'winTerm privacy validation passed.' -ForegroundColor Green
}
catch
{
    Write-Error "Privacy validation failed: $($_.Exception.Message)"
    exit 1
}
