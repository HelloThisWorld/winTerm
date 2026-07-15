# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [string[]]$Path,

    [Parameter()]
    [switch]$BundledFixtures,

    [Parameter()]
    [string]$StorageRoot,

    [Parameter()]
    [switch]$AllNamed,

    [Parameter()]
    [switch]$LastSession,

    [Parameter()]
    [switch]$RebuildIndex,

    [Parameter()]
    [switch]$Repair,

    [Parameter(DontShow)]
    [switch]$ExpectInvalid,

    [Parameter(DontShow)]
    [switch]$AllowLegacy
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function ConvertFrom-WorkspaceJson
{
    param(
        [Parameter(Mandatory)]
        [string]$Content
    )

    if ((Get-Command ConvertFrom-Json).Parameters.ContainsKey('DateKind'))
    {
        return $Content | ConvertFrom-Json -DateKind String -ErrorAction Stop
    }
    return $Content | ConvertFrom-Json -ErrorAction Stop
}

$maximumFileSize = 5MB
$maximumWindows = 32
$maximumTabsPerWindow = 128
$maximumPanesPerTab = 64
$maximumTotalPanes = 512
$maximumLayoutDepth = 32
$forbiddenFields = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
@(
    'autoRun', 'command', 'commandline', 'credential', 'credentials', 'environment',
    'environmentVariables', 'executable', 'executablePath', 'externalFile', 'include',
    'includes', 'password', 'plugin', 'plugins', 'privateKey', 'privateKeyPath',
    'registry', 'script', 'shellArguments', 'sshPrivateKey', 'startupCommand', 'token', 'url'
) | ForEach-Object { [void]$forbiddenFields.Add($_) }

function Get-PropertyValue
{
    param(
        [Parameter(Mandatory)]
        [object]$InputObject,

        [Parameter(Mandatory)]
        [string]$Name,

        [Parameter()]
        [object]$Default = $null
    )

    $property = $InputObject.PSObject.Properties[$Name]
    if ($null -eq $property)
    {
        return $Default
    }
    return $property.Value
}

function Test-SecurityNode
{
    param(
        [Parameter(Mandatory)]
        [AllowNull()]
        [object]$Node,

        [Parameter(Mandatory)]
        [string]$JsonPath,

        [Parameter(Mandatory)]
        [int]$Depth
    )

    if ($Depth -gt ($maximumLayoutDepth + 16))
    {
        throw "Workspace JSON nesting exceeds the supported limit at '$JsonPath'."
    }
    if ($null -eq $Node)
    {
        return
    }
    if ($Node -is [string])
    {
        if ($Node.Length -gt 16384)
        {
            throw "Workspace string exceeds the supported limit at '$JsonPath'."
        }
        if ($Node -match '^(?i:https?|file)://')
        {
            throw "External URL or file reference is forbidden at '$JsonPath'."
        }
        return
    }
    if ($Node -is [System.Collections.IEnumerable] -and $Node -isnot [pscustomobject])
    {
        $index = 0
        foreach ($item in $Node)
        {
            Test-SecurityNode -Node $item -JsonPath "$JsonPath[$index]" -Depth ($Depth + 1)
            $index++
        }
        return
    }
    foreach ($property in $Node.PSObject.Properties)
    {
        if ($forbiddenFields.Contains($property.Name))
        {
            throw "Forbidden import field '$($property.Name)' at '$JsonPath'."
        }
        Test-SecurityNode -Node $property.Value -JsonPath "$JsonPath.$($property.Name)" -Depth ($Depth + 1)
    }
}

function Test-Identifier
{
    param(
        [Parameter(Mandatory)]
        [string]$Value,

        [Parameter(Mandatory)]
        [string]$Field,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.HashSet[string]]$Identifiers
    )

    if ([string]::IsNullOrWhiteSpace($Value) -or $Value -match '[/\\\x00-\x1f]')
    {
        throw "Invalid identifier at '$Field'."
    }
    if (-not $Identifiers.Add($Value))
    {
        throw "Duplicate identifier '$Value' at '$Field'."
    }
}

function Test-Rect
{
    param(
        [Parameter(Mandatory)]
        [object]$Rect,

        [Parameter(Mandatory)]
        [string]$Field
    )

    foreach ($name in @('x', 'y', 'width', 'height'))
    {
        $value = Get-PropertyValue -InputObject $Rect -Name $name
        if ($null -eq $value -or $value -isnot [ValueType])
        {
            throw "Bounds field '$Field.$name' is missing or invalid."
        }
        $number = [double]$value
        if ([double]::IsNaN($number) -or [double]::IsInfinity($number))
        {
            throw "Bounds field '$Field.$name' must be finite."
        }
        if (($name -eq 'width' -or $name -eq 'height') -and $number -le 0)
        {
            throw "Bounds field '$Field.$name' must be positive."
        }
    }
}

function Test-Layout
{
    param(
        [Parameter(Mandatory)]
        [object]$Node,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.HashSet[string]]$PaneIds,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.HashSet[string]]$ReferencedPaneIds,

        [Parameter(Mandatory)]
        [int]$Depth,

        [Parameter(Mandatory)]
        [string]$Field
    )

    if ($Depth -gt $maximumLayoutDepth)
    {
        throw "Pane layout exceeds the supported nesting depth at '$Field'."
    }
    $type = [string](Get-PropertyValue -InputObject $Node -Name 'type')
    if ($type -eq 'pane')
    {
        $paneId = [string](Get-PropertyValue -InputObject $Node -Name 'paneId')
        if (-not $PaneIds.Contains($paneId))
        {
            throw "Layout references missing pane '$paneId' at '$Field'."
        }
        if (-not $ReferencedPaneIds.Add($paneId))
        {
            throw "Layout references pane '$paneId' more than once."
        }
        return
    }
    if ($type -ne 'split')
    {
        throw "Unsupported layout node '$type' at '$Field'."
    }
    $orientation = [string](Get-PropertyValue -InputObject $Node -Name 'orientation')
    if ($orientation -notin @('horizontal', 'vertical'))
    {
        throw "Unsupported split orientation at '$Field'."
    }
    $ratio = [double](Get-PropertyValue -InputObject $Node -Name 'ratio' -Default ([double]::NaN))
    if ([double]::IsNaN($ratio) -or [double]::IsInfinity($ratio))
    {
        throw "Split ratio must be finite at '$Field'."
    }
    $first = Get-PropertyValue -InputObject $Node -Name 'first'
    $second = Get-PropertyValue -InputObject $Node -Name 'second'
    if ($null -eq $first -or $null -eq $second)
    {
        throw "Split node is missing a child at '$Field'."
    }
    Test-Layout -Node $first -PaneIds $PaneIds -ReferencedPaneIds $ReferencedPaneIds -Depth ($Depth + 1) -Field "$Field.first"
    Test-Layout -Node $second -PaneIds $PaneIds -ReferencedPaneIds $ReferencedPaneIds -Depth ($Depth + 1) -Field "$Field.second"
}

function Test-WorkspaceDocument
{
    param(
        [Parameter(Mandatory)]
        [object]$Document,

        [Parameter(Mandatory)]
        [string]$SourcePath,

        [Parameter()]
        [switch]$PermitLegacy
    )

    Test-SecurityNode -Node $Document -JsonPath '$' -Depth 0
    $schemaVersion = Get-PropertyValue -InputObject $Document -Name 'schemaVersion'
    if ($null -eq $schemaVersion -or $schemaVersion -isnot [ValueType])
    {
        throw "Workspace schema version is missing in '$SourcePath'."
    }
    if ([int]$schemaVersion -gt 1)
    {
        throw "Workspace '$SourcePath' was created by a newer version of winTerm."
    }
    if ([int]$schemaVersion -lt 1)
    {
        if ($PermitLegacy)
        {
            return [pscustomobject]@{ Path = $SourcePath; Schema = [int]$schemaVersion; Windows = 0; Tabs = 0; Panes = 0; Result = 'Legacy migration input' }
        }
        throw "Workspace '$SourcePath' requires migration before it can be loaded."
    }

    $identifiers = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    Test-Identifier -Value ([string](Get-PropertyValue -InputObject $Document -Name 'id')) -Field 'id' -Identifiers $identifiers
    if ([string]::IsNullOrWhiteSpace([string](Get-PropertyValue -InputObject $Document -Name 'name')))
    {
        throw "Workspace name is missing in '$SourcePath'."
    }
    $windows = @(Get-PropertyValue -InputObject $Document -Name 'windows' -Default @())
    if ($windows.Count -lt 1 -or $windows.Count -gt $maximumWindows)
    {
        throw "Workspace window count is outside the supported range in '$SourcePath'."
    }

    $windowIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $tabCount = 0
    $paneCount = 0
    foreach ($window in $windows)
    {
        $windowId = [string](Get-PropertyValue -InputObject $window -Name 'id')
        Test-Identifier -Value $windowId -Field 'windows.id' -Identifiers $identifiers
        [void]$windowIds.Add($windowId)
        Test-Rect -Rect (Get-PropertyValue -InputObject $window -Name 'bounds') -Field 'window.bounds'
        Test-Rect -Rect (Get-PropertyValue -InputObject $window -Name 'normalizedBounds') -Field 'window.normalizedBounds'
        $monitor = Get-PropertyValue -InputObject $window -Name 'monitor'
        Test-Rect -Rect (Get-PropertyValue -InputObject $monitor -Name 'workArea') -Field 'window.monitor.workArea'
        $state = [string](Get-PropertyValue -InputObject $window -Name 'windowState' -Default 'normal')
        if ($state -notin @('normal', 'maximized', 'fullscreen', 'minimized'))
        {
            throw "Unsupported window state '$state'."
        }
        $tabs = @(Get-PropertyValue -InputObject $window -Name 'tabs' -Default @())
        if ($tabs.Count -lt 1 -or $tabs.Count -gt $maximumTabsPerWindow)
        {
            throw "Window '$windowId' has an unsupported tab count."
        }
        $tabIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        foreach ($tab in $tabs)
        {
            $tabCount++
            $tabId = [string](Get-PropertyValue -InputObject $tab -Name 'id')
            Test-Identifier -Value $tabId -Field 'tabs.id' -Identifiers $identifiers
            [void]$tabIds.Add($tabId)
            $panes = @(Get-PropertyValue -InputObject $tab -Name 'panes' -Default @())
            if ($panes.Count -lt 1 -or $panes.Count -gt $maximumPanesPerTab)
            {
                throw "Tab '$tabId' has an unsupported pane count."
            }
            $paneIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
            foreach ($pane in $panes)
            {
                $paneCount++
                $paneId = [string](Get-PropertyValue -InputObject $pane -Name 'id')
                Test-Identifier -Value $paneId -Field 'panes.id' -Identifiers $identifiers
                [void]$paneIds.Add($paneId)
                $directory = Get-PropertyValue -InputObject $pane -Name 'workingDirectory'
                $kind = [string](Get-PropertyValue -InputObject $directory -Name 'kind' -Default 'unknown')
                if ($kind -notin @('unknown', 'windows', 'unc', 'wsl', 'remote'))
                {
                    throw "Pane '$paneId' uses an unsupported working directory kind."
                }
                if ($kind -eq 'wsl')
                {
                    $directoryValue = [string](Get-PropertyValue -InputObject $directory -Name 'value')
                    if ($directoryValue -and -not $directoryValue.StartsWith('/'))
                    {
                        throw "Pane '$paneId' uses a Windows path in the WSL namespace."
                    }
                }
            }
            $referencedPaneIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
            Test-Layout -Node (Get-PropertyValue -InputObject $tab -Name 'layout') -PaneIds $paneIds -ReferencedPaneIds $referencedPaneIds -Depth 1 -Field 'tab.layout'
            if ($referencedPaneIds.Count -ne $paneIds.Count)
            {
                throw "Tab '$tabId' contains a pane that is not referenced exactly once."
            }
            $activePaneId = [string](Get-PropertyValue -InputObject $tab -Name 'activePaneId')
            if ($activePaneId -and -not $paneIds.Contains($activePaneId))
            {
                throw "Tab '$tabId' references a missing active pane."
            }
        }
        $activeTabId = [string](Get-PropertyValue -InputObject $window -Name 'activeTabId')
        if ($activeTabId -and -not $tabIds.Contains($activeTabId))
        {
            throw "Window '$windowId' references a missing active tab."
        }
    }
    if ($paneCount -gt $maximumTotalPanes)
    {
        throw "Workspace '$SourcePath' contains too many panes."
    }
    $activeWindowId = [string](Get-PropertyValue -InputObject $Document -Name 'activeWindowId')
    if ($activeWindowId -and -not $windowIds.Contains($activeWindowId))
    {
        throw "Workspace '$SourcePath' references a missing active window."
    }
    return [pscustomobject]@{ Path = $SourcePath; Schema = 1; Windows = $windows.Count; Tabs = $tabCount; Panes = $paneCount; Result = 'Valid' }
}

function Read-AndValidateWorkspace
{
    param(
        [Parameter(Mandatory)]
        [string]$WorkspacePath,

        [Parameter()]
        [switch]$PermitLegacy
    )

    $file = Get-Item -LiteralPath $WorkspacePath -ErrorAction Stop
    if ($file.Length -gt $maximumFileSize)
    {
        throw "Workspace '$WorkspacePath' exceeds the 5 MB import limit."
    }
    $content = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
    try
    {
        $document = ConvertFrom-WorkspaceJson -Content $content
    }
    catch
    {
        throw "Workspace '$WorkspacePath' is not valid JSON."
    }
    return Test-WorkspaceDocument -Document $document -SourcePath $file.FullName -PermitLegacy:$PermitLegacy
}

function Write-IndexAtomically
{
    param(
        [Parameter(Mandatory)]
        [string]$WorkspaceStorageRoot,

        [Parameter(Mandatory)]
        [object[]]$NamedWorkspaces
    )

    $indexPath = Join-Path $WorkspaceStorageRoot 'index.json'
    $temporaryPath = "$indexPath.tmp"
    $entries = foreach ($workspace in $NamedWorkspaces)
    {
        [ordered]@{
            id = [string](Get-PropertyValue -InputObject $workspace -Name 'id')
            name = [string](Get-PropertyValue -InputObject $workspace -Name 'name')
            path = "$(Get-PropertyValue -InputObject $workspace -Name 'id').json"
            updatedAt = [string](Get-PropertyValue -InputObject $workspace -Name 'updatedAt')
            isDefault = [bool](Get-PropertyValue -InputObject $workspace -Name 'isDefault' -Default $false)
            health = 'valid'
        }
    }
    $index = [ordered]@{ schemaVersion = 1; workspaces = @($entries) }
    $json = $index | ConvertTo-Json -Depth 10
    [System.IO.File]::WriteAllText($temporaryPath, $json + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $temporaryPath -Destination $indexPath -Force
}

try
{
    if ($Repair)
    {
        throw 'Automatic repair is intentionally unavailable in this source validator; no files were modified.'
    }

    $repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $fixtureRoot = Join-Path $repositoryRoot 'tests\winterm\Fixtures\Workspaces'
    $pathsToValidate = [System.Collections.Generic.List[string]]::new()
    if ($Path)
    {
        foreach ($item in $Path) { $pathsToValidate.Add((Resolve-Path -LiteralPath $item).Path) }
    }
    if ($AllNamed -or $LastSession -or $RebuildIndex)
    {
        if ([string]::IsNullOrWhiteSpace($StorageRoot))
        {
            throw '-StorageRoot is required for named, last-session, or index operations.'
        }
        if ($AllNamed -or $RebuildIndex)
        {
            $namedRoot = Join-Path $StorageRoot 'named'
            if (Test-Path -LiteralPath $namedRoot -PathType Container)
            {
                Get-ChildItem -LiteralPath $namedRoot -File -Filter '*.json' | ForEach-Object { $pathsToValidate.Add($_.FullName) }
            }
        }
        if ($LastSession)
        {
            $pathsToValidate.Add((Join-Path $StorageRoot 'last-session.json'))
        }
    }

    $useBundledFixtures = $BundledFixtures -or ($pathsToValidate.Count -eq 0 -and -not $RebuildIndex)
    $results = [System.Collections.Generic.List[object]]::new()
    if ($useBundledFixtures)
    {
        Get-ChildItem -LiteralPath (Join-Path $fixtureRoot 'Valid') -File -Filter '*.json' | ForEach-Object {
            $results.Add((Read-AndValidateWorkspace -WorkspacePath $_.FullName))
        }
        Get-ChildItem -LiteralPath (Join-Path $fixtureRoot 'Legacy') -File -Filter '*.json' | ForEach-Object {
            $results.Add((Read-AndValidateWorkspace -WorkspacePath $_.FullName -PermitLegacy))
        }
        foreach ($folder in @('Invalid', 'Corrupted', 'Security'))
        {
            Get-ChildItem -LiteralPath (Join-Path $fixtureRoot $folder) -File -Filter '*.json' | ForEach-Object {
                $fixturePath = $_.FullName
                try
                {
                    [void](Read-AndValidateWorkspace -WorkspacePath $fixturePath)
                    throw "Fixture '$fixturePath' was expected to be rejected."
                }
                catch
                {
                    if ($_.Exception.Message -like '*was expected to be rejected*') { throw }
                    $results.Add([pscustomobject]@{ Path = $fixturePath; Schema = '-'; Windows = '-'; Tabs = '-'; Panes = '-'; Result = 'Rejected as expected' })
                }
            }
        }
    }

    foreach ($workspacePath in $pathsToValidate)
    {
        try
        {
            $result = Read-AndValidateWorkspace -WorkspacePath $workspacePath -PermitLegacy:$AllowLegacy
            if ($ExpectInvalid)
            {
                throw "Workspace '$workspacePath' was expected to be rejected."
            }
            $results.Add($result)
        }
        catch
        {
            if (-not $ExpectInvalid) { throw }
            $results.Add([pscustomobject]@{ Path = $workspacePath; Schema = '-'; Windows = '-'; Tabs = '-'; Panes = '-'; Result = 'Rejected as expected' })
        }
    }

    if ($RebuildIndex)
    {
        $documents = foreach ($workspacePath in $pathsToValidate)
        {
            ConvertFrom-WorkspaceJson -Content (Get-Content -LiteralPath $workspacePath -Raw -Encoding UTF8)
        }
        Write-IndexAtomically -WorkspaceStorageRoot $StorageRoot -NamedWorkspaces @($documents)
    }

    $results | Format-Table -AutoSize
    Write-Host "PASS: validated $($results.Count) workspace fixture or file result(s)." -ForegroundColor Green
}
catch
{
    Write-Error "Workspace validation failed: $($_.Exception.Message)"
    exit 1
}
