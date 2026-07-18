# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$X64PackagePath,

    [Parameter(Mandatory)]
    [string]$Arm64PackagePath,

    [Parameter(Mandatory)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$stagingDirectory = $null

try
{
    $makeAppx = Get-Command makeappx.exe -ErrorAction SilentlyContinue
    if ($null -eq $makeAppx)
    {
        throw 'makeappx.exe was not found.'
    }

    $x64 = Get-Item -LiteralPath (Resolve-Path -LiteralPath $X64PackagePath).Path
    $arm64 = Get-Item -LiteralPath (Resolve-Path -LiteralPath $Arm64PackagePath).Path
    foreach ($package in @($x64, $arm64))
    {
        if ($package.Extension -ne '.msix' -or ($package.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)
        {
            throw "Invalid MSIX bundle input '$($package.Name)'."
        }
    }

    $resolvedOutput = if ([IO.Path]::IsPathRooted($OutputPath))
    {
        [IO.Path]::GetFullPath($OutputPath)
    }
    else
    {
        [IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputPath))
    }
    if (Test-Path -LiteralPath $resolvedOutput)
    {
        throw "Bundle output already exists: $resolvedOutput"
    }
    $outputParent = Split-Path -Parent $resolvedOutput
    if (-not (Test-Path -LiteralPath $outputParent))
    {
        New-Item -ItemType Directory -Path $outputParent | Out-Null
    }

    $stagingDirectory = Join-Path ([IO.Path]::GetTempPath()) ("winterm-bundle-{0}" -f [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stagingDirectory | Out-Null
    Copy-Item -LiteralPath $x64.FullName -Destination (Join-Path $stagingDirectory 'winTerm-1.0.0-x64.msix')
    Copy-Item -LiteralPath $arm64.FullName -Destination (Join-Path $stagingDirectory 'winTerm-1.0.0-arm64.msix')

    & $makeAppx.Source bundle /d $stagingDirectory /p $resolvedOutput /o
    if ($LASTEXITCODE -ne 0)
    {
        throw "makeappx.exe failed with exit code $LASTEXITCODE."
    }
    Write-Host "Created MSIX bundle: $resolvedOutput" -ForegroundColor Green
}
catch
{
    Write-Error "MSIX bundle generation failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    if ($null -ne $stagingDirectory -and (Test-Path -LiteralPath $stagingDirectory))
    {
        Remove-Item -LiteralPath $stagingDirectory -Recurse -Force
    }
}
