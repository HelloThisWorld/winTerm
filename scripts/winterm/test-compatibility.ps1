# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

& (Join-Path $PSScriptRoot 'test-shell-integration.ps1') -Shell WindowsPowerShell
if ($LASTEXITCODE -ne 0)
{
    exit $LASTEXITCODE
}
