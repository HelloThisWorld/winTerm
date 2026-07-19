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
    $wingetWorkflowPath = Join-Path $repositoryRoot '.github\workflows\winget.yml'
    $wingetWorkflow = Get-Content -LiteralPath $wingetWorkflowPath -Raw
    $wingetGeneratorPath = Join-Path $repositoryRoot 'scripts\winterm\generate-winget-manifests.ps1'
    $wingetGenerator = Get-Content -LiteralPath $wingetGeneratorPath -Raw

    foreach ($required in @(
        "push:",
        "- 'v1.0.2'",
        'permissions:',
        'contents: write',
        'id-token: write',
        'attestations: write',
        'build-local-development.ps1',
        '-ReleaseAsset',
        '-RequireSelfSigned',
        'winTerm-1.0.2-x64.msix',
        'winTerm-1.0.2.cer',
        'INSTALL.txt',
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
        $workflow.Contains('--clobber') -or
        $workflow.Contains('WINTERM_SIGNING_PFX'))
    {
        throw 'Release workflow contains a forbidden trigger, permission, or asset replacement option.'
    }

    foreach ($workflowToInspect in @($workflow, $wingetWorkflow))
    {
        foreach ($line in $workflowToInspect -split "`r?`n")
        {
            if ($line -match '^\s*uses:\s*(?<action>[^@\s]+)@(?<reference>[^\s#]+)')
            {
                if ($Matches.reference -notmatch '^[0-9a-f]{40}$')
                {
                    throw "GitHub Action '$($Matches.action)' is not pinned to an immutable commit SHA."
                }
            }
        }
    }

    if ($workflow -match '(?m)^\s*path:\s+.*\*\*' -or $workflow -match '(?m)^\s*path:\s+.*\/\*\s*$')
    {
        throw 'Release workflow must not upload broad workspace globs.'
    }

    $expectedWinGetInstallerPattern = "^https://github\.com/HelloThisWorld/winTerm/releases/download/v1\.0\.2/winTerm-1\.0\.2-x64\.msix$"
    if (-not $wingetGenerator.Contains("[ValidatePattern('$expectedWinGetInstallerPattern')]") -or
        $wingetGenerator.Contains('/v1\.0\.1/winTerm-1\.0\.1-x64\.msix'))
    {
        throw 'WinGet manifest generation does not accept only the current v1.0.2 public installer URL.'
    }

    foreach ($required in @(
        'pull_request:',
        'Microsoft.WinGet.Client',
        "-RequiredVersion '1.29.280'",
        "Repair-WinGetPackageManager -Version 'v1.29.280' -Force",
        "winget validate --manifest 'packaging\winget\1.0.2' --disable-interactivity"
    ))
    {
        if (-not $wingetWorkflow.Contains($required))
        {
            throw "WinGet workflow is missing required validator boundary '$required'."
        }
    }
    if ($wingetWorkflow.Contains('Repair-WinGetPackageManager -Latest'))
    {
        throw 'WinGet workflow must pin the manifest validator instead of installing the latest version.'
    }

    Write-Host 'PASS: stable release workflow security and publication boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Release workflow validation failed: $($_.Exception.Message)"
    exit 1
}
