# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$oversizedPath = Join-Path ([System.IO.Path]::GetTempPath()) ("winterm-oversized-{0}.json" -f [guid]::NewGuid().ToString('N'))
try
{
    $repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $fixtureRoot = Join-Path $repositoryRoot 'tests\winterm\Fixtures\Workspaces'
    & (Join-Path $PSScriptRoot 'validate-workspaces.ps1') -Path (Join-Path $fixtureRoot 'Valid\basic.winterm-workspace.json')
    if (-not $?) { throw 'The valid import fixture was rejected.' }

    foreach ($fixture in Get-ChildItem -LiteralPath (Join-Path $fixtureRoot 'Security') -File -Filter '*.json')
    {
        & (Join-Path $PSScriptRoot 'validate-workspaces.ps1') -Path $fixture.FullName -ExpectInvalid
        if (-not $?) { throw "Security fixture '$($fixture.Name)' did not produce the expected rejection." }
    }

    $stream = [System.IO.File]::Open($oversizedPath, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try
    {
        $stream.SetLength(5MB + 1)
    }
    finally
    {
        $stream.Dispose()
    }
    & (Join-Path $PSScriptRoot 'validate-workspaces.ps1') -Path $oversizedPath -ExpectInvalid
    if (-not $?) { throw 'The oversized import fixture did not produce the expected rejection.' }

    $importer = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Workspaces\ImportExport\WorkspaceImportExport.cpp') -Raw
    $serializer = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Workspaces\Persistence\WorkspaceSerializer.cpp') -Raw
    foreach ($required in @('Forbidden field', 'NewerSchemaPreserved', 'MaximumWorkspaceFileSize', '<USER_HOME>', 'External URL or file reference'))
    {
        if (-not ($importer.Contains($required) -or $serializer.Contains($required)))
        {
            throw "Import security boundary '$required' is missing."
        }
    }
    Write-Host 'PASS: workspace import size, command, executable, URL, and redaction boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Workspace import tests failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    if (Test-Path -LiteralPath $oversizedPath)
    {
        Remove-Item -LiteralPath $oversizedPath -Force
    }
}
