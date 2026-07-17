# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [Parameter()]
    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [switch]$RequireCompiled
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

try
{
    $root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $transformer = Get-Content -Raw -LiteralPath (Join-Path $root 'src\winterm\Docking\Layout\LayoutTransformer.cpp')
    $tests = Get-Content -Raw -LiteralPath (Join-Path $root 'src\cascadia\UnitTests_SettingsModel\WinTermDockingTests.cpp')
    foreach ($required in @(
        'DockZone::Left',
        'DockZone::Right',
        'DockZone::Top',
        'DockZone::Bottom',
        'DockZone::TopLeft',
        'DockZone::TopRight',
        'DockZone::BottomLeft',
        'DockZone::BottomRight',
        'LayoutNodeDescriptor::EmptySlot',
        'LayoutValidator::Validate'
    ))
    {
        if (-not $transformer.Contains($required))
        {
            throw "Layout transformer boundary '$required' is missing."
        }
    }
    foreach ($required in @(
        'EdgeDockingIsDeterministic',
        'CornerDockingCreatesExpectedEmptySlot',
        'RemovingPanePromotesSibling',
        'EmptySlotReplacementDoesNotAddSplit',
        'GeometryUsesTransformedLayout'
    ))
    {
        if (-not $tests.Contains($required))
        {
            throw "Compiled layout test '$required' is missing."
        }
    }

    $testBinary = Join-Path $root "bin\$Platform\$Configuration\UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll"
    if (Test-Path -LiteralPath $testBinary -PathType Leaf)
    {
        & (Join-Path $PSScriptRoot 'test.ps1') -Suite Relevant -Configuration $Configuration -Platform $Platform
        if (-not $?)
        {
            throw 'Compiled layout transformation tests failed.'
        }
    }
    elseif ($RequireCompiled)
    {
        throw "Compiled Settings Model tests were not found at '$testBinary'."
    }
    else
    {
        Write-Host 'SKIP: compiled layout transformation tests are unavailable.' -ForegroundColor Yellow
    }
    Write-Host 'PASS: layout transformation source and test boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Layout transformation tests failed: $($_.Exception.Message)"
    exit 1
}
