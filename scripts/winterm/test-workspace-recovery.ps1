# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-AtomicUtf8
{
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$Content
    )

    $temporaryPath = "$Path.tmp"
    [System.IO.File]::WriteAllText($temporaryPath, $Content, [System.Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporaryPath -Destination $Path -Force
}

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("winterm-recovery-test-{0}" -f [guid]::NewGuid().ToString('N'))
try
{
    New-Item -ItemType Directory -Path $temporaryRoot -Force | Out-Null
    $snapshots = Join-Path $temporaryRoot 'snapshots'
    New-Item -ItemType Directory -Path $snapshots -Force | Out-Null
    $fixture = Join-Path ((Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path) 'tests\winterm\Fixtures\Workspaces\Valid\basic.winterm-workspace.json'
    $content = Get-Content -LiteralPath $fixture -Raw -Encoding UTF8

    $lastSession = Join-Path $temporaryRoot 'last-session.json'
    $backup = Join-Path $temporaryRoot 'last-session.backup.json'
    Write-AtomicUtf8 -Path $lastSession -Content $content
    Copy-Item -LiteralPath $lastSession -Destination $backup
    Write-AtomicUtf8 -Path $lastSession -Content '{ invalid'
    $backupDocument = Get-Content -LiteralPath $backup -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($backupDocument.id -ne 'workspace.fixture-basic')
    {
        throw 'The previous valid workspace backup was not retained.'
    }

    foreach ($generation in 1..5)
    {
        Write-AtomicUtf8 -Path (Join-Path $snapshots ("snapshot-{0:D3}.json" -f $generation)) -Content $content
    }
    $snapshotFiles = @(Get-ChildItem -LiteralPath $snapshots -File -Filter 'snapshot-*.json' | Sort-Object Name -Descending)
    $snapshotFiles | Select-Object -Skip 3 | Remove-Item -Force
    if (@(Get-ChildItem -LiteralPath $snapshots -File -Filter 'snapshot-*.json').Count -ne 3)
    {
        throw 'Snapshot retention did not keep exactly the newest three files.'
    }

    $repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $store = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Workspaces\Persistence\WorkspaceStore.cpp') -Raw
    $runtime = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Workspaces\Runtime\WorkspaceRuntime.cpp') -Raw
    foreach ($required in @('last-session.backup.json', 'MarkSessionRunning', 'MarkCleanShutdown', 'LoadLatestValidSnapshot', '_latestWrittenGeneration'))
    {
        if (-not ($store.Contains($required) -or $runtime.Contains($required)))
        {
            throw "Recovery boundary '$required' is missing."
        }
    }
    Write-Host 'PASS: atomic backup, snapshot retention, generation, and recovery marker foundations.' -ForegroundColor Green
}
catch
{
    Write-Error "Workspace recovery tests failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    if (Test-Path -LiteralPath $temporaryRoot)
    {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
