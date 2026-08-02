# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-Class
{
    param(
        [Parameter(Mandatory)]
        [string]$Expected,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$ChangedFiles,

        [Parameter()]
        [string[]]$Labels = @(),

        [Parameter()]
        [ValidateSet('auto', 'quick', 'build', 'delivery', 'fast', 'full')]
        [string]$Mode = 'auto'
    )

    $actual = Get-WinTermChangeClassification -ChangedFiles $ChangedFiles -Labels $Labels -Mode $Mode
    if ($actual.ChangeClass -cne $Expected)
    {
        throw "Expected '$Expected' for '$($ChangedFiles -join ', ')' but selected '$($actual.ChangeClass)'."
    }
}

try
{
    Import-Module (Join-Path $PSScriptRoot 'ci\ChangeClassification.psm1') -Force

    Assert-Class -Expected 'docs-only' -ChangedFiles @('README.md', 'docs/architecture/progress.md', '.github/ISSUE_TEMPLATE/bug.md')
    Assert-Class -Expected 'validation-only' -ChangedFiles @('src/cascadia/TerminalApp/Pane.cpp')
    Assert-Class -Expected 'validation-only' -ChangedFiles @('assets/winterm/icons/winterm-32.png')
    Assert-Class -Expected 'validation-only' -ChangedFiles @('.github/workflows/winterm-validation.yml')
    Assert-Class -Expected 'validation-only' -ChangedFiles @('src/winterm/Branding/version.json')
    Assert-Class -Expected 'validation-only' -ChangedFiles @('packaging/inno/winTerm.iss')
    Assert-Class -Expected 'validation-only' -ChangedFiles @('src/cascadia/TerminalApp/TerminalAppLib.vcxproj')
    Assert-Class -Expected 'validation-only' -ChangedFiles @('docs/releases/1.1.3.md')
    Assert-Class -Expected 'validation-only' -ChangedFiles @()
    Assert-Class -Expected 'build' -ChangedFiles @('src/cascadia/TerminalApp/Pane.cpp') -Labels @('build')
    Assert-Class -Expected 'delivery' -ChangedFiles @('src/cascadia/TerminalApp/Pane.cpp') -Labels @('delivery')
    Assert-Class -Expected 'delivery' -ChangedFiles @('README.md') -Labels @('ci:full')
    Assert-Class -Expected 'validation-only' -ChangedFiles @() -Mode quick
    Assert-Class -Expected 'build' -ChangedFiles @() -Mode build
    Assert-Class -Expected 'delivery' -ChangedFiles @() -Mode delivery
    Assert-Class -Expected 'build' -ChangedFiles @() -Mode fast
    Assert-Class -Expected 'delivery' -ChangedFiles @() -Mode full

    $docs = Get-WinTermChangeClassification -ChangedFiles @('README.md')
    if ($docs.RunReleaseDelivery -or $docs.RunDebugValidation)
    {
        throw 'Documentation-only changes must not select native build or package jobs.'
    }
    $codeWithoutLabel = Get-WinTermChangeClassification -ChangedFiles @('src/cascadia/TerminalApp/Pane.cpp')
    if ($codeWithoutLabel.RunReleaseDelivery -or $codeWithoutLabel.RunDebugValidation)
    {
        throw 'Code changes without a native-build label must select quick validation only.'
    }

    $build = Get-WinTermChangeClassification -ChangedFiles @('src/cascadia/TerminalApp/Pane.cpp') -Labels @('build')
    if (-not $build.RunReleaseDelivery -or $build.RunDebugValidation)
    {
        throw 'The build label must select Release delivery without Debug validation.'
    }

    $delivery = Get-WinTermChangeClassification -ChangedFiles @('src/cascadia/TerminalApp/Pane.cpp') -Labels @('delivery')
    if (-not $delivery.RunReleaseDelivery -or -not $delivery.RunDebugValidation)
    {
        throw 'The delivery label must select Release delivery plus Debug validation.'
    }

    Write-Host 'PASS: CI change classification and execution modes.' -ForegroundColor Green
}
catch
{
    Write-Error "CI classification tests failed: $($_.Exception.Message)"
    exit 1
}
