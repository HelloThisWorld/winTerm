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
        [ValidateSet('auto', 'fast', 'full')]
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
    Assert-Class -Expected 'code' -ChangedFiles @('src/cascadia/TerminalApp/Pane.cpp')
    Assert-Class -Expected 'code' -ChangedFiles @('assets/winterm/icons/winterm-32.png')
    Assert-Class -Expected 'delivery' -ChangedFiles @('.github/workflows/winterm-validation.yml')
    Assert-Class -Expected 'delivery' -ChangedFiles @('src/winterm/Branding/version.json')
    Assert-Class -Expected 'delivery' -ChangedFiles @('packaging/inno/winTerm.iss')
    Assert-Class -Expected 'delivery' -ChangedFiles @('src/cascadia/TerminalApp/TerminalAppLib.vcxproj')
    Assert-Class -Expected 'delivery' -ChangedFiles @('docs/releases/1.1.3.md')
    Assert-Class -Expected 'delivery' -ChangedFiles @('README.md') -Labels @('ci:full')
    Assert-Class -Expected 'code' -ChangedFiles @() -Mode fast
    Assert-Class -Expected 'delivery' -ChangedFiles @() -Mode full

    $docs = Get-WinTermChangeClassification -ChangedFiles @('README.md')
    if ($docs.RunFastBuild -or $docs.RunFullBuild -or $docs.RunPackage)
    {
        throw 'Documentation-only changes must not select native build or package jobs.'
    }
    $code = Get-WinTermChangeClassification -ChangedFiles @('src/cascadia/TerminalApp/Pane.cpp')
    if (-not $code.RunFastBuild -or $code.RunFullBuild -or $code.RunPackage)
    {
        throw 'Code changes must select only the fast Release build.'
    }
    $delivery = Get-WinTermChangeClassification -ChangedFiles @('.github/workflows/winterm-validation.yml')
    if ($delivery.RunFastBuild -or -not $delivery.RunFullBuild -or -not $delivery.RunPackage)
    {
        throw 'Delivery changes must select full build and packaging without duplicating the Release-only job.'
    }

    Write-Host 'PASS: CI change classification and execution modes.' -ForegroundColor Green
}
catch
{
    Write-Error "CI classification tests failed: $($_.Exception.Message)"
    exit 1
}
