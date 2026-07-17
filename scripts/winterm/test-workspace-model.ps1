# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try
{
    & (Join-Path $PSScriptRoot 'validate-workspaces.ps1') -BundledFixtures
    if (-not $?)
    {
        throw 'Bundled workspace fixture validation failed.'
    }

    $repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $model = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Workspaces\Model\WorkspaceDescriptor.h') -Raw
    $validator = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Workspaces\Persistence\WorkspaceValidator.cpp') -Raw
    $serializer = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\winterm\Workspaces\Persistence\WorkspaceSerializer.cpp') -Raw

    foreach ($required in @('WorkspaceSchemaVersion{ 2 }', 'DockingModelVersion{ 1 }', 'MaximumWorkspacePanes{ 512 }', 'LayoutNodeType', 'EmptySlot', 'WorkingDirectoryKind'))
    {
        if (-not $model.Contains($required))
        {
            throw "Workspace model boundary '$required' is missing."
        }
    }
    foreach ($required in @('circularLayout', 'duplicateId', 'missingPaneReference', 'splitRatioAdjusted', 'invalidActiveWindow'))
    {
        if (-not $validator.Contains($required))
        {
            throw "Workspace validator rule '$required' is missing."
        }
    }
    foreach ($required in @('allowSpecialFloats', 'rejectDupKeys', 'MaximumWorkspaceFileSize', 'emitUTF8'))
    {
        if (-not $serializer.Contains($required))
        {
            throw "Workspace serializer safety option '$required' is missing."
        }
    }
    Write-Host 'PASS: workspace model, schema, validation, and serialization foundations.' -ForegroundColor Green
}
catch
{
    Write-Error "Workspace model tests failed: $($_.Exception.Message)"
    exit 1
}
