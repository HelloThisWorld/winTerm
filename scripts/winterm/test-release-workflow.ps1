# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

try
{
    $workflow = Get-Content -LiteralPath (Join-Path $repositoryRoot '.github\workflows\release.yml') -Raw
    $wingetWorkflow = Get-Content -LiteralPath (Join-Path $repositoryRoot '.github\workflows\winget.yml') -Raw
    $fullBuildWorkflow = Get-Content -LiteralPath (Join-Path $repositoryRoot '.github\workflows\winterm-full-build.yml') -Raw
    $wingetGenerator = Get-Content -LiteralPath (Join-Path $repositoryRoot 'scripts\winterm\generate-winget-manifests.ps1') -Raw

    foreach ($required in @(
        "- 'v*'",
        'contents: write',
        'id-token: write',
        'attestations: write',
        'verify-version.ps1 -RequireTag',
        "verify-branding.ps1 -ExpectedPublisher 'CN=helloThisWorld'",
        'build-unpackaged.ps1',
        'build-installer.ps1',
        'build-portable.ps1',
        'generate-release-artifacts.ps1',
        'verify-release-assets.ps1',
        'test-installer.ps1',
        'test-portable.ps1',
        '-RunSilentRoundTrip',
        '-RunAllUsersRoundTrip',
        '-RunDefaultPathRoundTrips',
        'winTerm-$env:WINTERM_VERSION-setup-x64.exe',
        'winTerm-$env:WINTERM_VERSION-portable-x64.zip',
        "'release', 'create'",
        '--draft',
        'gh release download',
        '--draft=false',
        'Publish only after every Draft gate passes',
        'Re-download and test the public Release',
        '9C73C3BAE7ED48D44112A0F48E66742C00090BDB5BEF71D9D3C056C66E97B732',
        'Pyrsys B\.V\.'
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
        $workflow -match '(?i)\.msix(bundle)?' -or
        $workflow -match '(?i)self-signed')
    {
        throw 'Release workflow contains a forbidden trigger, permission, replacement option, or primary MSIX path.'
    }
    if (-not $workflow.Contains('WINTERM_SIGNING_PFX_BASE64') -or
        -not $workflow.Contains('WINTERM_SIGNING_STATUS=unsigned') -or
        -not $workflow.Contains('WINTERM_SIGNING_STATUS=trusted-signed'))
    {
        throw 'Release workflow does not implement optional trusted signing with an explicit unsigned fallback.'
    }

    foreach ($workflowToInspect in @($workflow, $wingetWorkflow, $fullBuildWorkflow))
    {
        foreach ($line in $workflowToInspect -split "`r?`n")
        {
            if ($line -match '^\s*uses:\s*(?<action>[^@\s]+)@(?<reference>[^\s#]+)' -and
                $Matches.reference -notmatch '^[0-9a-f]{40}$')
            {
                throw "GitHub Action '$($Matches.action)' is not pinned to an immutable commit SHA."
            }
        }
    }

    if ($workflow -match '(?m)^\s*path:\s+.*\*\*' -or $workflow -match '(?m)^\s*path:\s+.*\/\*\s*$')
    {
        throw 'Release workflow must not upload broad workspace globs.'
    }

    foreach ($required in @(
        'release:',
        '- published',
        'workflow_dispatch:',
        'gh release view',
        'gh release download',
        'winTerm-$version-setup-x64.exe',
        'generate-winget-manifests.ps1',
        'Microsoft.WinGet.Client',
        "-RequiredVersion '1.29.280'",
        "Repair-WinGetPackageManager -Version 'v1.29.280' -AllUsers -Force",
        'winget validate --manifest $env:WINTERM_WINGET_OUTPUT --disable-interactivity'
    ))
    {
        if (-not $wingetWorkflow.Contains($required))
        {
            throw "WinGet workflow is missing required boundary '$required'."
        }
    }
    if ($wingetWorkflow.Contains('Repair-WinGetPackageManager -Latest') -or
        $wingetWorkflow -match '(?i)\.msix(bundle)?')
    {
        throw 'WinGet workflow must use the pinned validator and public Setup EXE only.'
    }

    foreach ($required in @(
        "[ValidatePattern('^https://github\.com/HelloThisWorld/winTerm/releases/download/.+\.exe$')]",
        'InstallerType: inno',
        'Scope: user',
        'Scope: machine',
        'Custom: /CURRENTUSER',
        'Custom: /ALLUSERS',
        'Publisher: helloThisWorld',
        'ManifestVersion: 1.12.0'
    ))
    {
        if (-not $wingetGenerator.Contains($required))
        {
            throw "WinGet generator is missing required Inno boundary '$required'."
        }
    }
    if ($wingetGenerator -match '(?i)InstallerType:\s*msix')
    {
        throw 'WinGet generator still declares MSIX as the installer type.'
    }

    foreach ($required in @(
        'build-unpackaged.ps1',
        'build-installer.ps1',
        'build-portable.ps1',
        'test-installer.ps1',
        'test-portable.ps1',
        '-RunSilentRoundTrip',
        '-RunAllUsersRoundTrip',
        '-RunDefaultPathRoundTrips',
        'winTerm-${{ env.WINTERM_VERSION }}-setup-x64.exe',
        'winTerm-${{ env.WINTERM_VERSION }}-portable-x64.zip'
    ))
    {
        if (-not $fullBuildWorkflow.Contains($required))
        {
            throw "Full-build CI is missing required unpackaged distribution gate '$required'."
        }
    }

    Write-Host 'PASS: unpackaged release and WinGet workflow security and publication boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Release workflow validation failed: $($_.Exception.Message)"
    exit 1
}
