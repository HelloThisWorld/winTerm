# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$InstallerPath,

    [Parameter()]
    [ValidateSet('x64', 'ARM64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [string]$Version,

    [Parameter()]
    [switch]$RunSilentRoundTrip,

    [Parameter()]
    [switch]$RunAllUsersRoundTrip,

    [Parameter()]
    [switch]$RunDefaultPathRoundTrips
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$testInstallRoots = [System.Collections.Generic.List[string]]::new()
$dataMarkerPaths = [System.Collections.Generic.List[string]]::new()

function Get-WindowsTerminalState
{
    $packagesAvailable = $true
    try
    {
        $packages = @(
            Get-AppxPackage -Name 'Microsoft.WindowsTerminal*' -ErrorAction Stop |
                Sort-Object -Property PackageFullName |
                Select-Object Name, PackageFullName, InstallLocation
        )
    }
    catch [System.PlatformNotSupportedException]
    {
        $packagesAvailable = $false
        $packages = @()
    }
    catch
    {
        if ($_.Exception.Message -notmatch 'Operation is not supported on this platform')
        {
            throw
        }
        $packagesAvailable = $false
        $packages = @()
    }
    $command = Get-Command wt.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    $commandPath = if ($null -ne $command) { [string]$command.Source } else { '' }
    $commandHash = ''
    if (-not [string]::IsNullOrWhiteSpace($commandPath) -and (Test-Path -LiteralPath $commandPath -PathType Leaf))
    {
        try
        {
            $commandHash = (Get-FileHash -LiteralPath $commandPath -Algorithm SHA256).Hash
        }
        catch
        {
            # App Execution Alias placeholders can resolve as files while
            # intentionally denying direct reads. Their path is still compared.
        }
    }

    return [ordered]@{
        packagesAvailable = $packagesAvailable
        packages = @($packages)
        commandPath = $commandPath
        commandHash = $commandHash
    } | ConvertTo-Json -Depth 5 -Compress
}

function Assert-InstalledPayload
{
    param(
        [Parameter()]
        [string]$InstallRoot
    )

    foreach ($name in @('winTerm.exe', 'winterm.exe', 'WindowsTerminal.exe', 'unins000.exe', 'version.json'))
    {
        if (-not (Test-Path -LiteralPath (Join-Path $InstallRoot $name) -PathType Leaf))
        {
            throw "Installed payload is missing '$name'."
        }
    }
}

function Assert-CommandRegistration
{
    param(
        [Parameter(Mandatory)]
        [ValidateSet('CurrentUser', 'AllUsers')]
        [string]$Scope,

        [Parameter(Mandatory)]
        [string]$InstallRoot,

        [Parameter(Mandatory)]
        [bool]$ShouldExist
    )

    $registryPath = if ($Scope -eq 'CurrentUser')
    {
        'Registry::HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\App Paths\winterm.exe'
    }
    else
    {
        'Registry::HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\App Paths\winterm.exe'
    }
    $exists = Test-Path -LiteralPath $registryPath
    if ($ShouldExist -and -not $exists)
    {
        throw "$Scope winterm App Paths registration is missing."
    }
    if (-not $ShouldExist -and $exists)
    {
        throw "$Scope winterm App Paths registration remained after uninstall."
    }
    if ($ShouldExist)
    {
        $registeredPath = [string](Get-Item -LiteralPath $registryPath).GetValue('')
        $expectedPath = Join-Path $InstallRoot 'winterm.exe'
        if (-not $registeredPath.Equals($expectedPath, [StringComparison]::OrdinalIgnoreCase))
        {
            throw "winterm App Paths points to '$registeredPath' instead of '$expectedPath'."
        }
    }
}

function Assert-StartMenuShortcut
{
    param(
        [Parameter(Mandatory)]
        [ValidateSet('CurrentUser', 'AllUsers')]
        [string]$Scope,

        [Parameter(Mandatory)]
        [bool]$ShouldExist
    )

    $programs = if ($Scope -eq 'CurrentUser')
    {
        [Environment]::GetFolderPath([Environment+SpecialFolder]::Programs)
    }
    else
    {
        [Environment]::GetFolderPath([Environment+SpecialFolder]::CommonPrograms)
    }
    $shortcut = Join-Path $programs 'winTerm\winTerm.lnk'
    $exists = Test-Path -LiteralPath $shortcut -PathType Leaf
    if ($ShouldExist -and -not $exists)
    {
        throw "$Scope Start Menu shortcut is missing at '$shortcut'."
    }
    if (-not $ShouldExist -and $exists)
    {
        throw "$Scope Start Menu shortcut remained after uninstall."
    }
}

function Invoke-Installer
{
    param(
        [Parameter(Mandatory)]
        [string]$FilePath,

        [Parameter(Mandatory)]
        [ValidateSet('CurrentUser', 'AllUsers')]
        [string]$Scope,

        [Parameter()]
        [string]$InstallRoot = ''
    )

    $scopeArgument = if ($Scope -eq 'CurrentUser') { '/CURRENTUSER' } else { '/ALLUSERS' }
    $arguments = @(
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART',
        $scopeArgument,
        '/TASKS=startmenu,command'
    )
    if (-not [string]::IsNullOrWhiteSpace($InstallRoot))
    {
        $arguments += "/DIR=`"$InstallRoot`""
    }
    $process = Start-Process -FilePath $FilePath -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0)
    {
        throw "$Scope silent installation failed with exit code $($process.ExitCode)."
    }
}

function Invoke-Uninstaller
{
    param(
        [Parameter(Mandatory)]
        [string]$InstallRoot
    )

    $uninstaller = Join-Path $InstallRoot 'unins000.exe'
    if (-not (Test-Path -LiteralPath $uninstaller -PathType Leaf))
    {
        throw "Uninstaller was not found at '$uninstaller'."
    }
    $process = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0)
    {
        throw "Silent uninstall failed with exit code $($process.ExitCode)."
    }
    Start-Sleep -Milliseconds 750
}

function Test-InstalledLaunch
{
    param(
        [Parameter(Mandatory)]
        [string]$InstallRoot,

        [Parameter()]
        [string[]]$CommandArguments = @(),

        [Parameter(Mandatory)]
        [string]$Description
    )

    $knownProcessIds = @(Get-Process -Name WindowsTerminal -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
    $knownConsoleIds = @(Get-Process -Name OpenConsole -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id)
    $launcherArguments = @{
        FilePath = Join-Path $InstallRoot 'winTerm.exe'
        PassThru = $true
    }
    if ($CommandArguments.Count -gt 0)
    {
        $launcherArguments.ArgumentList = $CommandArguments
    }
    $launcher = Start-Process @launcherArguments
    $launchedProcess = $null
    $consoleProcess = $null
    $deadline = [DateTime]::UtcNow.AddSeconds(20)
    while (($null -eq $launchedProcess -or $null -eq $consoleProcess) -and [DateTime]::UtcNow -lt $deadline)
    {
        Start-Sleep -Milliseconds 250
        if ($null -eq $launchedProcess)
        {
            foreach ($process in @(Get-Process -Name WindowsTerminal -ErrorAction SilentlyContinue))
            {
                if ($knownProcessIds -contains $process.Id) { continue }
                try
                {
                    $processPath = [string]$process.Path
                    $rootPrefix = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\') + '\'
                    if ($processPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase))
                    {
                        $launchedProcess = $process
                        break
                    }
                }
                catch
                {
                    # A process may exit between enumeration and path inspection.
                }
            }
        }
        if ($null -eq $consoleProcess)
        {
            foreach ($process in @(Get-Process -Name OpenConsole -ErrorAction SilentlyContinue))
            {
                if ($knownConsoleIds -contains $process.Id) { continue }
                try
                {
                    $processPath = [string]$process.Path
                    $rootPrefix = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\') + '\'
                    if ($processPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase))
                    {
                        $consoleProcess = $process
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
    if ($null -eq $launchedProcess)
    {
        if (-not $launcher.HasExited)
        {
            Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue
        }
        throw "The installed winTerm launcher did not create a host process for $Description."
    }
    if ($null -eq $consoleProcess)
    {
        Stop-Process -Id $launchedProcess.Id -Force -ErrorAction SilentlyContinue
        if (-not $launcher.HasExited)
        {
            Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue
        }
        throw "The installed winTerm launcher did not create OpenConsole.exe for $Description."
    }

    Stop-Process -Id $consoleProcess.Id -Force -ErrorAction SilentlyContinue
    Stop-Process -Id $launchedProcess.Id -Force -ErrorAction SilentlyContinue
    if (-not $launcher.HasExited)
    {
        Stop-Process -Id $launcher.Id -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Milliseconds 500
    Write-Host "PASS: installed launch with $Description." -ForegroundColor Green
}

function Test-InstallerRoundTrip
{
    param(
        [Parameter(Mandatory)]
        [string]$Installer,

        [Parameter(Mandatory)]
        [ValidateSet('CurrentUser', 'AllUsers')]
        [string]$Scope
    )

    if ($Scope -eq 'AllUsers')
    {
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        $principal = [Security.Principal.WindowsPrincipal]::new($identity)
        if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))
        {
            throw 'The all-users installer round trip requires an elevated PowerShell process.'
        }
    }

    $windowsTerminalBefore = Get-WindowsTerminalState
    $installRoot = Join-Path ([IO.Path]::GetTempPath()) ("winterm-install-test-{0}-{1}" -f $Scope.ToLowerInvariant(), [guid]::NewGuid().ToString('N'))
    [void]$script:testInstallRoots.Add($installRoot)

    Invoke-Installer -FilePath $Installer -Scope $Scope -InstallRoot $installRoot
    Assert-InstalledPayload -InstallRoot $installRoot
    Assert-CommandRegistration -Scope $Scope -InstallRoot $installRoot -ShouldExist $true
    Assert-StartMenuShortcut -Scope $Scope -ShouldExist $true
    Test-InstalledLaunch -InstallRoot $installRoot -Description 'the default profile'
    Test-InstalledLaunch -InstallRoot $installRoot -CommandArguments @('powershell.exe', '-NoLogo') -Description 'Windows PowerShell'
    Test-InstalledLaunch -InstallRoot $installRoot -CommandArguments @('cmd.exe') -Description 'Command Prompt'

    $dataRoot = Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)) 'winTerm'
    New-Item -ItemType Directory -Path $dataRoot -Force | Out-Null
    $dataMarkerPath = Join-Path $dataRoot ("installer-preservation-{0}.tmp" -f [guid]::NewGuid().ToString('N'))
    [void]$script:dataMarkerPaths.Add($dataMarkerPath)
    [IO.File]::WriteAllText($dataMarkerPath, 'preserve', [Text.UTF8Encoding]::new($false))

    Invoke-Installer -FilePath $Installer -Scope $Scope -InstallRoot $installRoot
    Assert-InstalledPayload -InstallRoot $installRoot
    if (-not (Test-Path -LiteralPath $dataMarkerPath -PathType Leaf))
    {
        throw 'The upgrade/repair installation removed existing winTerm user data.'
    }

    Invoke-Uninstaller -InstallRoot $installRoot
    Assert-CommandRegistration -Scope $Scope -InstallRoot $installRoot -ShouldExist $false
    Assert-StartMenuShortcut -Scope $Scope -ShouldExist $false
    if (Test-Path -LiteralPath (Join-Path $installRoot 'winTerm.exe'))
    {
        throw 'Silent uninstall left the main application binary behind.'
    }
    if (-not (Test-Path -LiteralPath $dataMarkerPath -PathType Leaf))
    {
        throw 'Silent uninstall removed winTerm user data without an explicit confirmation.'
    }

    Invoke-Installer -FilePath $Installer -Scope $Scope -InstallRoot $installRoot
    Assert-InstalledPayload -InstallRoot $installRoot
    Invoke-Uninstaller -InstallRoot $installRoot
    if (Test-Path -LiteralPath (Join-Path $installRoot 'winTerm.exe'))
    {
        throw 'Reinstall/uninstall verification left the main application binary behind.'
    }

    $windowsTerminalAfter = Get-WindowsTerminalState
    if ($windowsTerminalAfter -cne $windowsTerminalBefore)
    {
        throw 'The installer changed the existing Microsoft Windows Terminal package or wt.exe command state.'
    }
    Write-Host "PASS: $Scope install, launch, upgrade, uninstall, reinstall, registration, and data-preservation tests." -ForegroundColor Green
}

function Test-DefaultPathRoundTrip
{
    param(
        [Parameter(Mandatory)]
        [string]$Installer,

        [Parameter(Mandatory)]
        [ValidateSet('CurrentUser', 'AllUsers')]
        [string]$Scope
    )

    if ($Scope -eq 'AllUsers')
    {
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        $principal = [Security.Principal.WindowsPrincipal]::new($identity)
        if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator))
        {
            throw 'The all-users default-path round trip requires an elevated PowerShell process.'
        }
    }
    $installRoot = if ($Scope -eq 'CurrentUser')
    {
        Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)) 'Programs\winTerm'
    }
    else
    {
        Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFiles)) 'winTerm'
    }
    if (Test-Path -LiteralPath $installRoot)
    {
        throw "Refusing to overwrite an existing default installation directory: $installRoot"
    }

    $windowsTerminalBefore = Get-WindowsTerminalState
    Invoke-Installer -FilePath $Installer -Scope $Scope
    Assert-InstalledPayload -InstallRoot $installRoot
    Assert-CommandRegistration -Scope $Scope -InstallRoot $installRoot -ShouldExist $true
    Assert-StartMenuShortcut -Scope $Scope -ShouldExist $true
    Test-InstalledLaunch -InstallRoot $installRoot -Description "$Scope default-path installation"
    Invoke-Uninstaller -InstallRoot $installRoot
    Assert-CommandRegistration -Scope $Scope -InstallRoot $installRoot -ShouldExist $false
    Assert-StartMenuShortcut -Scope $Scope -ShouldExist $false
    if (Test-Path -LiteralPath (Join-Path $installRoot 'winTerm.exe'))
    {
        throw "$Scope default-path uninstall left the main application binary behind."
    }
    if ((Get-WindowsTerminalState) -cne $windowsTerminalBefore)
    {
        throw "$Scope default-path installation changed Microsoft Windows Terminal or wt.exe state."
    }
    Write-Host "PASS: $Scope default installation path, launch, registration, and uninstall." -ForegroundColor Green
}

$resolvedInstaller = (Resolve-Path -LiteralPath $InstallerPath).Path

try
{
    & (Join-Path $PSScriptRoot 'verify-release-layout.ps1') -Path $resolvedInstaller -Type Installer -Platform $Platform -Version $Version
    if (-not $?) { throw 'Static installer verification failed.' }

    if (-not $RunSilentRoundTrip -and -not $RunAllUsersRoundTrip -and -not $RunDefaultPathRoundTrips)
    {
        Write-Host 'SKIP: install/uninstall round trips were not requested.' -ForegroundColor Yellow
        return
    }
    if ($RunSilentRoundTrip)
    {
        Test-InstallerRoundTrip -Installer $resolvedInstaller -Scope CurrentUser
    }
    if ($RunAllUsersRoundTrip)
    {
        Test-InstallerRoundTrip -Installer $resolvedInstaller -Scope AllUsers
    }
    if ($RunDefaultPathRoundTrips)
    {
        Test-DefaultPathRoundTrip -Installer $resolvedInstaller -Scope CurrentUser
        Test-DefaultPathRoundTrip -Installer $resolvedInstaller -Scope AllUsers
    }
}
catch
{
    Write-Error "Installer test failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    foreach ($dataMarkerPath in $dataMarkerPaths)
    {
        if (Test-Path -LiteralPath $dataMarkerPath -PathType Leaf)
        {
            Remove-Item -LiteralPath $dataMarkerPath -Force
        }
    }
    $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    foreach ($testInstallRoot in $testInstallRoots)
    {
        if (Test-Path -LiteralPath $testInstallRoot)
        {
            $resolvedTestRoot = [IO.Path]::GetFullPath($testInstallRoot)
            if (-not $resolvedTestRoot.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase))
            {
                throw "Refusing to remove installer test directory outside the temporary root: $resolvedTestRoot"
            }
            Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
        }
    }
}
