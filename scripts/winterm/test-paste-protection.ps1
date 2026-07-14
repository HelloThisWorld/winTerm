# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$analyzer = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Clipboard\PasteRiskAnalyzer.cpp') -Raw
$unitTests = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\cascadia\UnitTests_SettingsModel\WinTermShellTests.cpp') -Raw

if (-not ($analyzer.Contains('AnalyzePasteRisk') -and $analyzer.Contains('SuspiciousCommand') -and $unitTests.Contains('PasteRiskAnalyzerClassifiesWithoutChangingText')))
{
    throw 'Paste protection source or its non-executing unit test is missing.'
}

Write-Host 'PASS: Paste protection source and non-executing unit test are present.' -ForegroundColor Green
