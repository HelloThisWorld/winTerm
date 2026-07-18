# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

try
{
    $workflowPath = Join-Path $repositoryRoot '.github\workflows\release.yml'
    $workflow = Get-Content -LiteralPath $workflowPath -Raw

    foreach ($required in @(
        "push:",
        "- 'v1.0.0'",
        'workflow_dispatch:',
        'environment: winterm-stable-release',
        'permissions:',
        'contents: write',
        'id-token: write',
        'attestations: write',
        'gh release create',
        '--draft',
        'verify-release-assets.ps1',
        'verify-checksums.ps1',
        'gh release download',
        '--draft=false',
        '--prerelease=false',
        '--latest'
    ))
    {
        if (-not $workflow.Contains($required))
        {
            throw "Release workflow is missing required boundary '$required'."
        }
    }

    if ($workflow -match '(?m)^\s*pull_request_target\s*:' -or
        $workflow -match '(?m)^\s*permissions:\s*write-all\s*$' -or
        $workflow.Contains('--clobber'))
    {
        throw 'Release workflow contains a forbidden trigger, permission, or asset replacement option.'
    }

    foreach ($line in $workflow -split "`r?`n")
    {
        if ($line -match '^\s*uses:\s*(?<action>[^@\s]+)@(?<reference>[^\s#]+)')
        {
            if ($Matches.reference -notmatch '^[0-9a-f]{40}$')
            {
                throw "GitHub Action '$($Matches.action)' is not pinned to an immutable commit SHA."
            }
        }
    }

    if ($workflow -match '(?m)^\s*path:\s+.*\*\*' -or $workflow -match '(?m)^\s*path:\s+.*\/\*\s*$')
    {
        throw 'Release workflow must not upload broad workspace globs.'
    }

    Write-Host 'PASS: stable release workflow security and publication boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Release workflow validation failed: $($_.Exception.Message)"
    exit 1
}
