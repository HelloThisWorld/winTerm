# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [switch]$RequireTag
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Assert-Condition
{
    param(
        [Parameter(Mandatory)]
        [bool]$Condition,

        [Parameter(Mandatory)]
        [string]$Message
    )

    if (-not $Condition)
    {
        throw $Message
    }
    Write-Host "PASS: $Message" -ForegroundColor Green
}

function Get-Text
{
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    $path = Join-Path $script:repositoryRoot $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf))
    {
        throw "Versioned file is missing: $RelativePath"
    }
    return Get-Content -LiteralPath $path -Raw
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

try
{
    $versionPath = Join-Path $repositoryRoot 'src\winterm\Branding\version.json'
    $version = Get-Content -LiteralPath $versionPath -Raw | ConvertFrom-Json

    Assert-Condition ($version.applicationVersion -eq '1.3.0') 'Application version is 1.3.0'
    Assert-Condition ($version.packageVersion -eq '1.3.0.7') 'Package version is 1.3.0.7'
    Assert-Condition ($version.moduleVersion -eq '1.3.0') 'PowerShell module version is 1.3.0'
    Assert-Condition ($version.modulePrerelease -eq '') 'PowerShell module has no prerelease suffix'

    # The release workflow treats any channel other than 'stable' as a
    # prerelease: it marks the GitHub Release --prerelease and --latest=false.
    # Keep the channel, the module prerelease suffix, and the application
    # version suffix consistent so a prerelease can never publish as Latest.
    $supportedChannels = @('stable', 'alpha', 'beta')
    Assert-Condition ($supportedChannels -contains $version.channel) "Release channel is one of: $($supportedChannels -join ', ')"
    $isPrerelease = $version.channel -ne 'stable'
    Assert-Condition ($isPrerelease -eq ($version.modulePrerelease -ne '')) 'Module prerelease suffix agrees with the release channel'
    Assert-Condition ($isPrerelease -eq ($version.applicationVersion -match '-')) 'Application version suffix agrees with the release channel'
    Assert-Condition ($version.packageVersion -match '^\d+\.\d+\.\d+\.\d+$') 'Package version stays a four-part numeric version'
    Assert-Condition ($version.moduleVersion -match '^\d+\.\d+\.\d+$') 'PowerShell module version stays numeric'

    Assert-Condition ($version.tag -eq 'v1.3.0') 'Stable tag is v1.3.0'
    Assert-Condition ($version.workspaceSchemaVersion -eq 2) 'Workspace Schema version remains 2'
    Assert-Condition ($version.dockingModelVersion -eq 1) 'Docking Model version remains 1'
    Assert-Condition ($version.shellProtocolVersion -eq 1) 'Shell Protocol version remains 1'
    Assert-Condition ($version.themeSchemaVersion -eq 1) 'Theme Schema version remains 1'
    Assert-Condition ($version.updateManifestSchemaVersion -eq 1) 'Update Manifest Schema version remains 1'
    Assert-Condition ($version.microsoftTerminalUpstreamRevision -match '^[0-9a-f]{40}$') 'Microsoft Terminal upstream revision is immutable'

    [xml]$manifest = Get-Text 'src\cascadia\CascadiaPackage\Package-winTerm.appxmanifest'
    $manifestNamespace = [System.Xml.XmlNamespaceManager]::new($manifest.NameTable)
    $manifestNamespace.AddNamespace('f', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
    $identity = $manifest.SelectSingleNode('/f:Package/f:Identity', $manifestNamespace)
    Assert-Condition ($identity.Version -eq $version.packageVersion) 'MSIX manifest version matches release metadata'

    [xml]$customProps = Get-Text 'custom.props'
    $propsNamespace = [System.Xml.XmlNamespaceManager]::new($customProps.NameTable)
    $propsNamespace.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')
    $major = $customProps.SelectSingleNode('//m:VersionMajor', $propsNamespace).'#text'
    $minor = $customProps.SelectSingleNode('//m:VersionMinor', $propsNamespace).'#text'
    Assert-Condition ("$major.$minor" -eq '1.3') 'Executable metadata major and minor versions match 1.3'

    $moduleManifest = Import-PowerShellDataFile -LiteralPath (Join-Path $repositoryRoot 'shell\powershell\winTerm.Shell\winTerm.Shell.psd1')
    Assert-Condition ($moduleManifest.ModuleVersion.ToString() -eq $version.moduleVersion) 'PowerShell manifest version matches release metadata'
    Assert-Condition ($moduleManifest.PrivateData.PSData.Prerelease -eq $version.modulePrerelease) 'PowerShell manifest prerelease matches release metadata'
    Assert-Condition ((Get-Text 'shell\powershell\winTerm.Shell\winTerm.Shell.psm1').Contains("`$script:WinTermModuleVersion = '1.3.0'")) 'PowerShell module runtime version matches release metadata'

    $shellVersion = Get-Text 'shell\shared\version.json' | ConvertFrom-Json
    Assert-Condition ($shellVersion.applicationVersion -eq $version.applicationVersion) 'Shell asset application version matches release metadata'
    Assert-Condition ($shellVersion.moduleVersion -eq $version.moduleVersion) 'Shell asset module version matches release metadata'
    Assert-Condition ($shellVersion.protocolVersion -eq $version.shellProtocolVersion) 'Shell asset protocol version matches release metadata'

    $releaseHeader = Get-Text 'src\winterm\Branding\ReleaseMetadata.h'
    Assert-Condition ($releaseHeader.Contains('ApplicationVersion{ L"1.3.0" }')) 'About metadata application version is 1.3.0'
    Assert-Condition ($releaseHeader.Contains('ReleaseChannel{ L"Stable" }')) 'About metadata channel is Stable'
    Assert-Condition ($releaseHeader.Contains($version.microsoftTerminalUpstreamRevision)) 'About metadata contains the Microsoft Terminal upstream revision'
    Assert-Condition ($releaseHeader.Contains('WorkspaceSchemaVersion{ 2 }')) 'About metadata contains Workspace Schema version 2'
    Assert-Condition ($releaseHeader.Contains('DockingModelVersion{ 1 }')) 'About metadata contains Docking Model version 1'
    Assert-Condition ($releaseHeader.Contains('ShellProtocolVersion{ 1 }')) 'About metadata contains Shell Protocol version 1'
    Assert-Condition ($releaseHeader.Contains('ThemeSchemaVersion{ 1 }')) 'About metadata contains Theme Schema version 1'

    $hostResource = Get-Text 'src\cascadia\WindowsTerminal\WindowsTerminal.rc'
    $packageVersionTuple = $version.packageVersion.Replace('.', ',')
    Assert-Condition ($hostResource.Contains("FILEVERSION $packageVersionTuple")) 'Terminal host file version matches release metadata'
    Assert-Condition ($hostResource.Contains("PRODUCTVERSION $packageVersionTuple")) 'Terminal host numeric product version matches release metadata'
    Assert-Condition ($hostResource.Contains("`"FileVersion`", `"$($version.packageVersion)\0`"")) 'Terminal host display file version matches release metadata'
    Assert-Condition ($hostResource.Contains("`"ProductVersion`", `"$($version.applicationVersion)\0`"")) 'Terminal host display product version matches release metadata'

    $launcherResource = Get-Text 'src\cascadia\wt\wt.rc'
    Assert-Condition ($launcherResource.Contains("FILEVERSION $packageVersionTuple")) 'winTerm launcher file version matches release metadata'
    Assert-Condition ($launcherResource.Contains("PRODUCTVERSION $packageVersionTuple")) 'winTerm launcher numeric product version matches release metadata'
    Assert-Condition ($launcherResource.Contains("`"FileVersion`", `"$($version.packageVersion)\0`"")) 'winTerm launcher display file version matches release metadata'
    Assert-Condition ($launcherResource.Contains("`"ProductVersion`", `"$($version.applicationVersion)\0`"")) 'winTerm launcher display product version matches release metadata'
    Assert-Condition ($launcherResource.Contains('"ProductName", "winTerm\0"')) 'winTerm launcher product name is winTerm'

    $shimResource = Get-Text 'src\winterm-tools\winterm-shim\winterm-shim.rc'
    Assert-Condition ($shimResource.Contains("FILEVERSION $packageVersionTuple")) 'Shell helper file version matches release metadata'
    Assert-Condition ($shimResource.Contains("PRODUCTVERSION $packageVersionTuple")) 'Shell helper numeric product version matches release metadata'
    Assert-Condition ($shimResource.Contains("`"FileVersion`", `"$($version.packageVersion)\0`"")) 'Shell helper display file version matches release metadata'
    Assert-Condition ($shimResource.Contains("`"ProductVersion`", `"$($version.applicationVersion)\0`"")) 'Shell helper display product version matches release metadata'
    Assert-Condition ($shimResource.Contains('"ProductName", "winTerm\0"')) 'Shell helper product name is winTerm'

    Assert-Condition ((Get-Text 'src\winterm\Workspaces\Model\WorkspaceDescriptor.h').Contains('WorkspaceSchemaVersion{ 2 }')) 'Workspace model remains at Schema version 2'
    Assert-Condition ((Get-Text 'src\winterm\Workspaces\Model\WorkspaceDescriptor.h').Contains('DockingModelVersion{ 1 }')) 'Workspace model remains at Docking version 1'
    Assert-Condition ((Get-Text 'src\winterm\Workspaces\Model\WorkspaceDescriptor.h').Contains('applicationVersion{ "1.3.0" }')) 'Workspace model application-version fallback is 1.3.0'
    Assert-Condition ((Get-Text 'src\winterm\Shell\Protocol\ShellIntegrationProtocol.h').Contains('ShellProtocolVersion{ 1 }')) 'Shell protocol remains at version 1'
    Assert-Condition ((Get-Text 'src\winterm\Appearance\Themes\ThemeDescriptor.h').Contains('CurrentThemeSchemaVersion{ 1 }')) 'Theme Schema remains at version 1'
    Assert-Condition ((Get-Text 'src\winterm\Workspaces\Persistence\WorkspaceSerializer.cpp').Contains('"1.3.0"')) 'Workspace serializer application-version fallback is 1.3.0'

    $releaseWorkflow = Get-Text '.github\workflows\release.yml'
    Assert-Condition ($releaseWorkflow.Contains("- 'v*'")) 'Release workflow accepts version tags through a generic guarded trigger'
    Assert-Condition ($releaseWorkflow.Contains('["v1.2.1","v1.2.2","v1.2.3","v1.2.4"]')) 'Release workflow identifies engineering checkpoint tags'
    Assert-Condition ($releaseWorkflow.Contains('checkpoint-validation:')) 'Release workflow retains quick validation for checkpoint tags'
    Assert-Condition ($releaseWorkflow.Contains("`$expectedTag = `"v`$(`$metadata.applicationVersion)`"")) 'Release workflow derives the expected tag from version.json'
    Assert-Condition ($releaseWorkflow.Contains("`$metadata.tag -cne `$expectedTag")) 'Release workflow rejects a version metadata tag mismatch'

    Assert-Condition ((Get-Text 'CHANGELOG.md').Contains('## 1.3.0 - 2026-08-13')) 'Changelog contains the 1.3.0 stable entry'
    Assert-Condition ((Get-Text 'README.md').Contains("current source version is ``$($version.applicationVersion)``")) 'README source version matches release metadata'
    Assert-Condition ((Get-Text 'README.md').Contains('latest stable release is `1.3.0`')) 'README stable version matches this release'

    if ($RequireTag)
    {
        $tag = (& git describe --tags --exact-match 2>$null).Trim()
        Assert-Condition ($LASTEXITCODE -eq 0 -and $tag -eq $version.tag) 'Checked-out commit is exactly tagged v1.3.0'
    }

    Write-Host 'winTerm version consistency verification passed.' -ForegroundColor Green
}
catch
{
    Write-Error "Version consistency verification failed: $($_.Exception.Message)"
    exit 1
}
