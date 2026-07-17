# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding(DefaultParameterSetName = 'WorkspaceFile')]
param(
    [Parameter(Mandatory, ParameterSetName = 'WorkspaceFile', Position = 0)]
    [Alias('Path')]
    [string]$WorkspaceFile,

    [Parameter(Mandatory, ParameterSetName = 'NamedWorkspace')]
    [string]$NamedWorkspace,

    [Parameter(Mandatory, ParameterSetName = 'LastSession')]
    [switch]$LastSession,

    [Parameter(Mandatory, ParameterSetName = 'TestFixture')]
    [string]$TestFixture,

    [Parameter(Mandatory, ParameterSetName = 'CurrentRuntime')]
    [switch]$CurrentRuntime,

    [Parameter()]
    [string]$StorageRoot,

    [Parameter()]
    [switch]$Normalize,

    [Parameter()]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$minimumRatio = 0.05
$maximumRatio = 0.95
$maximumDepth = 32

function Get-PropertyValue
{
    param(
        [Parameter(Mandatory)]
        [object]$InputObject,

        [Parameter(Mandatory)]
        [string]$Name,

        [Parameter()]
        [AllowNull()]
        [object]$Default = $null
    )

    $property = $InputObject.PSObject.Properties[$Name]
    if ($null -eq $property)
    {
        return $Default
    }
    return $property.Value
}

function Resolve-WorkspaceRoot
{
    if ([string]::IsNullOrWhiteSpace($StorageRoot))
    {
        return Join-Path (Join-Path $env:LOCALAPPDATA 'winTerm') 'workspaces'
    }
    $resolved = [System.IO.Path]::GetFullPath($StorageRoot)
    if ((Split-Path -Leaf $resolved) -ieq 'workspaces')
    {
        return $resolved
    }
    return Join-Path $resolved 'workspaces'
}

function Resolve-InputPath
{
    $repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    switch ($PSCmdlet.ParameterSetName)
    {
        'WorkspaceFile'
        {
            return [System.IO.Path]::GetFullPath($WorkspaceFile)
        }
        'NamedWorkspace'
        {
            if ($NamedWorkspace -notmatch '^[A-Za-z0-9._-]+$')
            {
                throw 'Named Workspace IDs may contain only letters, numbers, period, underscore, and hyphen.'
            }
            return Join-Path (Join-Path (Resolve-WorkspaceRoot) 'named') "$NamedWorkspace.json"
        }
        'LastSession'
        {
            return Join-Path (Resolve-WorkspaceRoot) 'last-session.json'
        }
        'CurrentRuntime'
        {
            return Join-Path (Resolve-WorkspaceRoot) 'current-layout.json'
        }
        'TestFixture'
        {
            if (Test-Path -LiteralPath $TestFixture -PathType Leaf)
            {
                return [System.IO.Path]::GetFullPath($TestFixture)
            }
            $fixtureRoot = Join-Path $repositoryRoot 'tests\winterm\Fixtures'
            $matches = @(Get-ChildItem -LiteralPath $fixtureRoot -Recurse -File | Where-Object {
                $_.Name -ieq $TestFixture -or $_.BaseName -ieq $TestFixture
            })
            if ($matches.Count -ne 1)
            {
                throw "Test fixture '$TestFixture' did not resolve to exactly one file."
            }
            return $matches[0].FullName
        }
        default
        {
            throw 'A layout source is required.'
        }
    }
}

function Add-Error
{
    param(
        [Parameter(Mandatory)]
        [object]$Stats,

        [Parameter(Mandatory)]
        [string]$Message
    )

    [void]$Stats.Errors.Add($Message)
}

function Test-LayoutNode
{
    param(
        [Parameter(Mandatory)]
        [AllowNull()]
        [object]$Node,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.HashSet[string]]$DeclaredPaneIds,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.HashSet[string]]$ReferencedPaneIds,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.HashSet[string]]$GlobalSlotIds,

        [Parameter(Mandatory)]
        [object]$Stats,

        [Parameter(Mandatory)]
        [int]$Depth,

        [Parameter(Mandatory)]
        [string]$Location
    )

    $Stats.MaximumDepth = [math]::Max($Stats.MaximumDepth, $Depth)
    if ($Depth -gt $maximumDepth)
    {
        Add-Error -Stats $Stats -Message "Maximum layout depth exceeded at '$Location'."
        return
    }
    if ($null -eq $Node)
    {
        $Stats.OrphanNodes++
        Add-Error -Stats $Stats -Message "Missing layout node at '$Location'."
        return
    }

    $Stats.NodeCount++
    $type = [string](Get-PropertyValue -InputObject $Node -Name 'type')
    switch ($type)
    {
        'pane'
        {
            $Stats.PaneCount++
            $paneId = [string](Get-PropertyValue -InputObject $Node -Name 'paneId')
            if (-not $ReferencedPaneIds.Add($paneId))
            {
                $Stats.DuplicateIds++
                Add-Error -Stats $Stats -Message "Pane '$paneId' is referenced more than once."
            }
            if (-not $DeclaredPaneIds.Contains($paneId))
            {
                $Stats.OrphanNodes++
                Add-Error -Stats $Stats -Message "Layout pane '$paneId' has no declared session."
            }
        }
        'emptySlot'
        {
            $Stats.EmptySlotCount++
            $slotId = [string](Get-PropertyValue -InputObject $Node -Name 'slotId')
            if ([string]::IsNullOrWhiteSpace($slotId) -or -not $GlobalSlotIds.Add($slotId))
            {
                $Stats.DuplicateIds++
                Add-Error -Stats $Stats -Message "Empty-slot ID '$slotId' is missing or duplicated."
            }
        }
        'split'
        {
            $ratioValue = Get-PropertyValue -InputObject $Node -Name 'ratio' -Default ([double]::NaN)
            $ratio = [double]$ratioValue
            if ([double]::IsNaN($ratio) -or [double]::IsInfinity($ratio) -or $ratio -lt $minimumRatio -or $ratio -gt $maximumRatio)
            {
                $Stats.InvalidRatios++
                Add-Error -Stats $Stats -Message "Split ratio is invalid at '$Location'."
            }
            Test-LayoutNode -Node (Get-PropertyValue -InputObject $Node -Name 'first') -DeclaredPaneIds $DeclaredPaneIds -ReferencedPaneIds $ReferencedPaneIds -GlobalSlotIds $GlobalSlotIds -Stats $Stats -Depth ($Depth + 1) -Location "$Location.first"
            Test-LayoutNode -Node (Get-PropertyValue -InputObject $Node -Name 'second') -DeclaredPaneIds $DeclaredPaneIds -ReferencedPaneIds $ReferencedPaneIds -GlobalSlotIds $GlobalSlotIds -Stats $Stats -Depth ($Depth + 1) -Location "$Location.second"
        }
        default
        {
            $Stats.OrphanNodes++
            Add-Error -Stats $Stats -Message "Unsupported layout node '$type' at '$Location'."
        }
    }
}

function Repair-LayoutNode
{
    param(
        [Parameter()]
        [AllowNull()]
        [object]$Node
    )

    if ($null -eq $Node)
    {
        return $null
    }
    $type = [string](Get-PropertyValue -InputObject $Node -Name 'type')
    if ($type -ne 'split')
    {
        return $Node
    }

    $first = Repair-LayoutNode -Node (Get-PropertyValue -InputObject $Node -Name 'first')
    $second = Repair-LayoutNode -Node (Get-PropertyValue -InputObject $Node -Name 'second')
    if ($null -eq $first -and $null -eq $second)
    {
        return $null
    }
    if ($null -eq $first)
    {
        return $second
    }
    if ($null -eq $second)
    {
        return $first
    }
    $Node.first = $first
    $Node.second = $second
    $ratio = [double](Get-PropertyValue -InputObject $Node -Name 'ratio' -Default 0.5)
    if ([double]::IsNaN($ratio) -or [double]::IsInfinity($ratio))
    {
        $ratio = 0.5
    }
    $Node.ratio = [math]::Min($maximumRatio, [math]::Max($minimumRatio, $ratio))
    return $Node
}

try
{
    if ($Normalize -and [string]::IsNullOrWhiteSpace($OutputPath))
    {
        throw '-Normalize requires -OutputPath. The source file is never modified in place.'
    }
    if (-not $Normalize -and -not [string]::IsNullOrWhiteSpace($OutputPath))
    {
        throw '-OutputPath is valid only with -Normalize.'
    }

    $inputPath = Resolve-InputPath
    if (-not (Test-Path -LiteralPath $inputPath -PathType Leaf))
    {
        throw "Layout source '$inputPath' was not found."
    }
    $content = Get-Content -LiteralPath $inputPath -Raw
    if ((Get-Command ConvertFrom-Json).Parameters.ContainsKey('DateKind'))
    {
        $workspace = $content | ConvertFrom-Json -DateKind String
    }
    else
    {
        $workspace = $content | ConvertFrom-Json
    }

    $stats = [pscustomobject]@{
        NodeCount = 0
        PaneCount = 0
        EmptySlotCount = 0
        DuplicateIds = 0
        OrphanNodes = 0
        OrphanSessions = 0
        InvalidRatios = 0
        MaximumDepth = 0
        Errors = [System.Collections.Generic.List[string]]::new()
    }
    $globalPaneIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    $globalSlotIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($window in @((Get-PropertyValue -InputObject $workspace -Name 'windows' -Default @())))
    {
        foreach ($tab in @((Get-PropertyValue -InputObject $window -Name 'tabs' -Default @())))
        {
            $declaredPaneIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
            foreach ($pane in @((Get-PropertyValue -InputObject $tab -Name 'panes' -Default @())))
            {
                $paneId = [string](Get-PropertyValue -InputObject $pane -Name 'id')
                if (-not $declaredPaneIds.Add($paneId) -or -not $globalPaneIds.Add($paneId))
                {
                    $stats.DuplicateIds++
                    Add-Error -Stats $stats -Message "Pane ID '$paneId' is declared more than once."
                }
            }
            $referencedPaneIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
            Test-LayoutNode -Node (Get-PropertyValue -InputObject $tab -Name 'layout') -DeclaredPaneIds $declaredPaneIds -ReferencedPaneIds $referencedPaneIds -GlobalSlotIds $globalSlotIds -Stats $stats -Depth 1 -Location 'tab.layout'
            foreach ($paneId in $declaredPaneIds)
            {
                if (-not $referencedPaneIds.Contains($paneId))
                {
                    $stats.OrphanSessions++
                    Add-Error -Stats $stats -Message "Declared pane '$paneId' is absent from its layout."
                }
            }
        }
    }

    $result = [pscustomobject]@{
        Path = $inputPath
        NodeCount = $stats.NodeCount
        PaneCount = $stats.PaneCount
        EmptySlotCount = $stats.EmptySlotCount
        DuplicateIds = $stats.DuplicateIds
        OrphanNodes = $stats.OrphanNodes
        OrphanSessions = $stats.OrphanSessions
        InvalidRatios = $stats.InvalidRatios
        MaximumDepth = $stats.MaximumDepth
        Result = if ($stats.Errors.Count -eq 0) { 'Valid' } else { 'Invalid' }
    }
    $result | Format-List
    foreach ($errorMessage in $stats.Errors)
    {
        Write-Warning $errorMessage
    }

    if ($Normalize)
    {
        $resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
        if ($resolvedOutput -ieq [System.IO.Path]::GetFullPath($inputPath))
        {
            throw 'Normalization output must not overwrite the source file.'
        }
        foreach ($window in @((Get-PropertyValue -InputObject $workspace -Name 'windows' -Default @())))
        {
            foreach ($tab in @((Get-PropertyValue -InputObject $window -Name 'tabs' -Default @())))
            {
                $tab.layout = Repair-LayoutNode -Node (Get-PropertyValue -InputObject $tab -Name 'layout')
            }
        }
        $outputDirectory = Split-Path -Parent $resolvedOutput
        if (-not [string]::IsNullOrWhiteSpace($outputDirectory))
        {
            [System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
        }
        $workspace | ConvertTo-Json -Depth 64 | Set-Content -LiteralPath $resolvedOutput -Encoding utf8
        Write-Host "Normalized layout written to '$resolvedOutput'. The source was not modified." -ForegroundColor Green
    }

    if ($stats.Errors.Count -gt 0)
    {
        exit 2
    }
}
catch
{
    Write-Error "Layout validation failed: $($_.Exception.Message)"
    exit 1
}
