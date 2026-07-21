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

    Assert-Condition ($version.applicationVersion -eq '0.7.0-beta.2') 'Application version is 0.7.0-beta.2'
    Assert-Condition ($version.packageVersion -eq '1.0.4.0') 'Package version is 1.0.4.0'
    Assert-Condition ($version.moduleVersion -eq '0.7.0') 'PowerShell module version is 0.7.0'
    Assert-Condition ($version.modulePrerelease -eq 'beta.2') 'PowerShell module prerelease is beta.2'
    Assert-Condition ($version.channel -eq 'beta') 'Release channel is beta'
    Assert-Condition ($version.tag -eq 'v0.7.0-beta.2') 'Beta tag is v0.7.0-beta.2'
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
    Assert-Condition ("$major.$minor" -eq '0.7') 'Executable metadata major and minor versions match 0.7'

    $moduleManifest = Import-PowerShellDataFile -LiteralPath (Join-Path $repositoryRoot 'shell\powershell\winTerm.Shell\winTerm.Shell.psd1')
    Assert-Condition ($moduleManifest.ModuleVersion.ToString() -eq $version.moduleVersion) 'PowerShell manifest version matches release metadata'
    Assert-Condition ($moduleManifest.PrivateData.PSData.Prerelease -eq $version.modulePrerelease) 'PowerShell manifest prerelease matches release metadata'
    Assert-Condition ((Get-Text 'shell\powershell\winTerm.Shell\winTerm.Shell.psm1').Contains("`$script:WinTermModuleVersion = '0.7.0-beta.2'")) 'PowerShell module runtime version matches release metadata'

    $shellVersion = Get-Text 'shell\shared\version.json' | ConvertFrom-Json
    Assert-Condition ($shellVersion.applicationVersion -eq $version.applicationVersion) 'Shell asset application version matches release metadata'
    Assert-Condition ($shellVersion.moduleVersion -eq $version.moduleVersion) 'Shell asset module version matches release metadata'
    Assert-Condition ($shellVersion.protocolVersion -eq $version.shellProtocolVersion) 'Shell asset protocol version matches release metadata'

    $releaseHeader = Get-Text 'src\winterm\Branding\ReleaseMetadata.h'
    Assert-Condition ($releaseHeader.Contains('ApplicationVersion{ L"0.7.0-beta.2" }')) 'About metadata application version is 0.7.0-beta.2'
    Assert-Condition ($releaseHeader.Contains('ReleaseChannel{ L"Beta" }')) 'About metadata channel is Beta'
    Assert-Condition ($releaseHeader.Contains($version.microsoftTerminalUpstreamRevision)) 'About metadata contains the Microsoft Terminal upstream revision'
    Assert-Condition ($releaseHeader.Contains('WorkspaceSchemaVersion{ 2 }')) 'About metadata contains Workspace Schema version 2'
    Assert-Condition ($releaseHeader.Contains('DockingModelVersion{ 1 }')) 'About metadata contains Docking Model version 1'
    Assert-Condition ($releaseHeader.Contains('ShellProtocolVersion{ 1 }')) 'About metadata contains Shell Protocol version 1'
    Assert-Condition ($releaseHeader.Contains('ThemeSchemaVersion{ 1 }')) 'About metadata contains Theme Schema version 1'

    $hostResource = Get-Text 'src\cascadia\WindowsTerminal\WindowsTerminal.rc'
    $packageVersionTuple = $version.packageVersion.Replace('.', ',')
    Assert-Condition ($hostResource.Contains("FILEVERSION $packageVersionTuple")) 'Terminal host file version matches release metadata'
    Assert-Condition ($hostResource.Contains("PRODUCTVERSION $packageVersionTuple")) 'Terminal host numeric product version matches release metadata'
    Assert-Condition ($hostResource.Contains("`"ProductVersion`", `"$($version.applicationVersion)\0`"")) 'Terminal host display product version matches release metadata'

    Assert-Condition ((Get-Text 'src\winterm\Workspaces\Model\WorkspaceDescriptor.h').Contains('WorkspaceSchemaVersion{ 2 }')) 'Workspace model remains at Schema version 2'
    Assert-Condition ((Get-Text 'src\winterm\Workspaces\Model\WorkspaceDescriptor.h').Contains('DockingModelVersion{ 1 }')) 'Workspace model remains at Docking version 1'
    Assert-Condition ((Get-Text 'src\winterm\Shell\Protocol\ShellIntegrationProtocol.h').Contains('ShellProtocolVersion{ 1 }')) 'Shell protocol remains at version 1'
    Assert-Condition ((Get-Text 'src\winterm\Appearance\Themes\ThemeDescriptor.h').Contains('CurrentThemeSchemaVersion{ 1 }')) 'Theme Schema remains at version 1'
    Assert-Condition ((Get-Text 'src\winterm\Workspaces\Persistence\WorkspaceSerializer.cpp').Contains('"0.7.0-beta.2"')) 'Workspace application-version fallback is 0.7.0-beta.2'

    $releaseWorkflow = Get-Text '.github\workflows\release.yml'
    Assert-Condition ($releaseWorkflow.Contains("- 'v*'")) 'Release workflow accepts version tags through a generic guarded trigger'
    Assert-Condition ($releaseWorkflow.Contains("`$expectedTag = `"v`$(`$metadata.applicationVersion)`"")) 'Release workflow derives the expected tag from version.json'
    Assert-Condition ($releaseWorkflow.Contains("`$metadata.tag -cne `$expectedTag")) 'Release workflow rejects a version metadata tag mismatch'

    Assert-Condition ((Get-Text 'CHANGELOG.md').Contains('## 0.7.0-beta.2')) 'Changelog contains 0.7.0-beta.2'

    if ($RequireTag)
    {
        $tag = (& git describe --tags --exact-match 2>$null).Trim()
        Assert-Condition ($LASTEXITCODE -eq 0 -and $tag -eq $version.tag) 'Checked-out commit is exactly tagged v0.7.0-beta.2'
    }

    Write-Host 'winTerm version consistency verification passed.' -ForegroundColor Green
}
catch
{
    Write-Error "Version consistency verification failed: $($_.Exception.Message)"
    exit 1
}
