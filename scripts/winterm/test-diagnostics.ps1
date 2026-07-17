# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$redactionHeader = Join-Path $repositoryRoot 'src\winterm\Diagnostics\Redaction.h'
$redactionSource = Join-Path $repositoryRoot 'src\winterm\Diagnostics\Redaction.cpp'

foreach ($path in @($redactionHeader, $redactionSource))
{
    if (-not (Test-Path -LiteralPath $path -PathType Leaf))
    {
        throw "Diagnostic redaction source is missing '$path'."
    }
}

$source = Get-Content -LiteralPath $redactionSource -Raw
foreach ($requiredMarker in @('<user>', '<server>', '<share>', '<email>', '<redacted>', '<secret>', '<connection-endpoint>'))
{
    if (-not $source.Contains($requiredMarker))
    {
        throw "Diagnostic redaction source does not handle '$requiredMarker'."
    }
}
if ($source -match 'WriteAllText|CreateFile|Temporary')
{
    throw 'Diagnostic redaction must not write unredacted content to temporary files.'
}

Write-Host 'PASS: diagnostic redaction source boundaries.' -ForegroundColor Green
