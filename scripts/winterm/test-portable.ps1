# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$PortablePath,

    [Parameter()]
    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [string]$Version,

    [Parameter()]
    [switch]$RunLaunch
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$extractRoot = $null

function Get-FileState
{
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf))
    {
        return 'missing'
    }
    try
    {
        $item = Get-Item -LiteralPath $Path
        return "$($item.Length):$((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash)"
    }
    catch
    {
        return 'present-unreadable'
    }
}

function Get-PortableIsolationState
{
    $registryPath = 'Registry::HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\App Paths\winterm.exe'
    $registryValue = if (Test-Path -LiteralPath $registryPath)
    {
        [string](Get-Item -LiteralPath $registryPath).GetValue('')
    }
    else
    {
        'missing'
    }
    $shortcut = Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::Programs)) 'winTerm\winTerm.lnk'
    $localSettings = Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)) 'winTerm\settings.json'
    return [ordered]@{
        appPaths = $registryValue
        startMenu = Get-FileState -Path $shortcut
        localSettings = Get-FileState -Path $localSettings
    } | ConvertTo-Json -Compress
}

function Stop-PortableProcesses
{
    param(
        [Parameter(Mandatory)]
        [string]$Root
    )

    $rootPrefix = [IO.Path]::GetFullPath($Root).TrimEnd('\\') + '\\'
    $processNames = @('winTerm', 'winterm', 'winterm-shim', 'WindowsTerminal', 'OpenConsole')
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do
    {
        $foundProcess = $false
        foreach ($processName in $processNames)
        {
            foreach ($process in @(Get-Process -Name $processName -ErrorAction SilentlyContinue))
            {
                try
                {
                    if ([string]$process.Path -and $process.Path.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase))
                    {
                        $foundProcess = $true
                        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
                    }
                }
                catch
                {
                    # A process may exit between enumeration and path inspection.
                }
            }
        }
        if ($foundProcess)
        {
            Start-Sleep -Milliseconds 200
        }
    }
    while ($foundProcess -and [DateTime]::UtcNow -lt $deadline)
}

try
{
    $resolvedPortable = (Resolve-Path -LiteralPath $PortablePath).Path
    & (Join-Path $PSScriptRoot 'verify-release-layout.ps1') -Path $resolvedPortable -Type Portable -Platform $Platform -Version $Version
    if (-not $?) { throw 'Portable layout verification failed.' }

    if (-not $RunLaunch)
    {
        Write-Host 'SKIP: Portable launch and data-mode test was not requested.' -ForegroundColor Yellow
        return
    }

    $isolationBefore = Get-PortableIsolationState
    $extractRoot = Join-Path ([IO.Path]::GetTempPath()) ("winterm-portable-test-{0}" -f [guid]::NewGuid().ToString('N'))
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::ExtractToDirectory($resolvedPortable, $extractRoot)

    $knownProcessIds = @(Get-Process -Name WindowsTerminal -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
    $launcher = Start-Process -FilePath (Join-Path $extractRoot 'winTerm.exe') -PassThru
    $hostProcess = $null
    $portableSettings = Join-Path $extractRoot 'data\settings.json'
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while ([DateTime]::UtcNow -lt $deadline -and ($null -eq $hostProcess -or -not (Test-Path -LiteralPath $portableSettings -PathType Leaf)))
    {
        Start-Sleep -Milliseconds 250
        if ($null -eq $hostProcess)
        {
            foreach ($process in @(Get-Process -Name WindowsTerminal -ErrorAction SilentlyContinue))
            {
                if ($knownProcessIds -contains $process.Id) { continue }
                try
                {
                    $rootPrefix = [IO.Path]::GetFullPath($extractRoot).TrimEnd('\') + '\'
                    if ([string]$process.Path -and $process.Path.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase))
                    {
                        $hostProcess = $process
                        break
                    }
                }
                catch
                {
                    # A process may exit between enumeration and path inspection.
                }
            }
        }
    }
    if ($null -eq $hostProcess)
    {
        throw 'The Portable launcher did not create a host process from the extracted directory.'
    }
    if (-not (Test-Path -LiteralPath $portableSettings -PathType Leaf))
    {
        throw 'Portable mode did not create data\settings.json beside the application.'
    }

    Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
    if (-not $launcher.HasExited)
    {
        Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue
    }
    $isolationAfter = Get-PortableIsolationState
    if ($isolationAfter -cne $isolationBefore)
    {
        throw 'Portable launch changed App Paths, the Start Menu shortcut, or local winTerm settings.'
    }
    Write-Host 'PASS: Portable extract, launch, adjacent data mode, and installation isolation.' -ForegroundColor Green
}
catch
{
    Write-Error "Portable test failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    if ($null -ne $extractRoot -and (Test-Path -LiteralPath $extractRoot))
    {
        $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        $resolvedExtractRoot = [IO.Path]::GetFullPath($extractRoot)
        if (-not $resolvedExtractRoot.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase))
        {
            throw "Refusing to remove Portable test directory outside the temporary root: $resolvedExtractRoot"
        }
        Stop-PortableProcesses -Root $resolvedExtractRoot
        Remove-Item -LiteralPath $resolvedExtractRoot -Recurse -Force
    }
}
