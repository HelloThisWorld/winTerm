# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try
{
    $repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    Import-Module (Join-Path $PSScriptRoot 'ci\ChangeClassification.psm1') -Force

    $mode = if ([string]::IsNullOrWhiteSpace($env:WINTERM_CI_MODE)) { 'auto' } else { $env:WINTERM_CI_MODE.ToLowerInvariant() }
    $eventName = if ([string]::IsNullOrWhiteSpace($env:WINTERM_CI_EVENT_NAME)) { 'workflow_dispatch' } else { $env:WINTERM_CI_EVENT_NAME }
    $labels = if ([string]::IsNullOrWhiteSpace($env:WINTERM_CI_LABELS_JSON)) { @() } else { @($env:WINTERM_CI_LABELS_JSON | ConvertFrom-Json) }
    $changedFiles = @()
    $diffDescription = ''

    Set-Location $repositoryRoot
    if ($mode -eq 'auto' -and $eventName -eq 'pull_request')
    {
        if ([string]::IsNullOrWhiteSpace($env:WINTERM_CI_BASE_SHA) -or [string]::IsNullOrWhiteSpace($env:WINTERM_CI_HEAD_SHA))
        {
            throw 'The pull request base and head SHAs are required for exact change classification.'
        }
        $diffDescription = "$env:WINTERM_CI_BASE_SHA...$env:WINTERM_CI_HEAD_SHA"
        $changedFiles = @(git diff --name-only --diff-filter=ACMRDTUXB $diffDescription)
        if ($LASTEXITCODE -ne 0)
        {
            throw "git diff failed for '$diffDescription'."
        }
    }
    elseif ($mode -eq 'auto' -and -not [string]::IsNullOrWhiteSpace($env:WINTERM_CI_DEFAULT_BRANCH))
    {
        $defaultRef = "origin/$env:WINTERM_CI_DEFAULT_BRANCH"
        $headSha = if ([string]::IsNullOrWhiteSpace($env:WINTERM_CI_HEAD_SHA)) { (git rev-parse HEAD).Trim() } else { $env:WINTERM_CI_HEAD_SHA }
        $mergeBase = (git merge-base $defaultRef $headSha).Trim()
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($mergeBase) -and $mergeBase -cne $headSha)
        {
            $diffDescription = "$mergeBase...$headSha"
            $changedFiles = @(git diff --name-only --diff-filter=ACMRDTUXB $diffDescription)
            if ($LASTEXITCODE -ne 0)
            {
                throw "git diff failed for '$diffDescription'."
            }
        }
        else
        {
            $mode = 'fast'
            $diffDescription = 'no applicable branch diff; auto selected fast validation'
        }
    }

    $classification = Get-WinTermChangeClassification -ChangedFiles $changedFiles -Labels $labels -Mode $mode
    $outputs = [ordered]@{
        change_class = $classification.ChangeClass
        run_fast_build = $classification.RunFastBuild.ToString().ToLowerInvariant()
        run_full_build = $classification.RunFullBuild.ToString().ToLowerInvariant()
        run_package = $classification.RunPackage.ToString().ToLowerInvariant()
    }
    if (-not [string]::IsNullOrWhiteSpace($env:GITHUB_OUTPUT))
    {
        foreach ($entry in $outputs.GetEnumerator())
        {
            "$($entry.Key)=$($entry.Value)" >> $env:GITHUB_OUTPUT
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($env:GITHUB_STEP_SUMMARY))
    {
        @(
            '## Change classification',
            '',
            "- Selected class: **$($classification.ChangeClass)**",
            "- Diff: $diffDescription",
            "- Changed files: $($classification.ChangedFiles.Count)",
            "- Reasons: $($classification.Reasons -join '; ')",
            '',
            '| Output | Value |',
            '| --- | --- |',
            ('| `change_class` | `{0}` |' -f $classification.ChangeClass),
            ('| `run_fast_build` | `{0}` |' -f $outputs.run_fast_build),
            ('| `run_full_build` | `{0}` |' -f $outputs.run_full_build),
            ('| `run_package` | `{0}` |' -f $outputs.run_package)
        ) | Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY
    }

    Write-Host "Selected CI class '$($classification.ChangeClass)': $($classification.Reasons -join '; ')" -ForegroundColor Green
}
catch
{
    Write-Error "CI change classification failed: $($_.Exception.Message)"
    exit 1
}
