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
    $requiredFiles = @(
        'src\winterm\Docking\Targets\DockTargetResolver.cpp',
        'src\winterm\Docking\Overlay\DockingOverlayModel.cpp',
        'src\winterm\Accessibility\KeyboardDockingController.cpp',
        'src\winterm\Settings\Docking\DockingSettingsModel.cpp',
        'src\winterm\Docking\Diagnostics\DockingDiagnostics.cpp',
        'src\cascadia\UnitTests_SettingsModel\WinTermDockingPresentationTests.cpp',
        'src\cascadia\UnitTests_SettingsModel\WinTermDockingDiagnosticsTests.cpp'
    )
    foreach ($relativePath in $requiredFiles)
    {
        if (-not (Test-Path -LiteralPath (Join-Path $root $relativePath) -PathType Leaf))
        {
            throw "Docking presentation boundary '$relativePath' is missing."
        }
    }

    $resolver = Get-Content -LiteralPath (Join-Path $root $requiredFiles[0]) -Raw
    $overlay = Get-Content -LiteralPath (Join-Path $root $requiredFiles[1]) -Raw
    $settings = Get-Content -LiteralPath (Join-Path $root $requiredFiles[3]) -Raw
    $diagnostics = Get-Content -LiteralPath (Join-Path $root $requiredFiles[4]) -Raw
    foreach ($required in @('TabStrip', 'EmptySlot', 'Pane', 'WindowContent', 'NewWindow'))
    {
        if (-not $resolver.Contains("DockTargetType::$required"))
        {
            throw "Target priority '$required' is missing."
        }
    }
    foreach ($required in @('DockPreview::Build', 'LayoutGeometry::Calculate', 'automationName', 'disabledReason'))
    {
        if (-not $overlay.Contains($required))
        {
            throw "Overlay or preview boundary '$required' is missing."
        }
    }
    if (-not ($settings.Contains('runtimeUiVerified') -and $settings.Contains('transactionRollbackVerified')))
    {
        throw 'The runtime readiness gate is missing.'
    }
    foreach ($forbidden in @('command text', 'clipboard', 'DragPayloadRecord'))
    {
        if ($diagnostics.Contains($forbidden))
        {
            throw "Docking diagnostics contain forbidden payload boundary '$forbidden'."
        }
    }

    $testBinary = Join-Path $root "bin\$Platform\$Configuration\UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll"
    if (Test-Path -LiteralPath $testBinary -PathType Leaf)
    {
        & (Join-Path $PSScriptRoot 'test.ps1') -Suite Relevant -Configuration $Configuration -Platform $Platform
        if (-not $?)
        {
            throw 'Compiled docking presentation tests failed.'
        }
    }
    elseif ($RequireCompiled)
    {
        throw "Compiled Settings Model tests were not found at '$testBinary'."
    }
    else
    {
        Write-Host 'SKIP: compiled docking presentation tests are unavailable.' -ForegroundColor Yellow
    }
    Write-Host 'PASS: docking target, overlay, keyboard, settings, and diagnostic source boundaries.' -ForegroundColor Green
}
catch
{
    Write-Error "Docking presentation tests failed: $($_.Exception.Message)"
    exit 1
}
