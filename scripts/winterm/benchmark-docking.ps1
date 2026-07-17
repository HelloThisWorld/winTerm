# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [ValidateRange(1, 10000)]
    [int]$Iterations = 100
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function New-PaneNode
{
    param([Parameter(Mandatory)][string]$Id)
    return [ordered]@{ type = 'pane'; paneId = $Id }
}

function New-WorkspaceModel
{
    param(
        [Parameter(Mandatory)][int]$WindowCount,
        [Parameter(Mandatory)][int]$TabCount,
        [Parameter(Mandatory)][int]$PaneCount
    )

    $windows = @(for ($windowIndex = 0; $windowIndex -lt $WindowCount; $windowIndex++)
    {
        $tabs = @(for ($tabIndex = $windowIndex; $tabIndex -lt $TabCount; $tabIndex += $WindowCount)
        {
            $paneId = "pane-$tabIndex-0"
            [ordered]@{
                id = "tab-$tabIndex"
                panes = @([ordered]@{ id = $paneId })
                layout = (New-PaneNode -Id $paneId)
            }
        })
        [ordered]@{ id = "window-$windowIndex"; tabs = @($tabs) }
    })
    for ($paneIndex = $TabCount; $paneIndex -lt $PaneCount; $paneIndex++)
    {
        $window = $windows[$paneIndex % $windows.Count]
        $tab = $window.tabs[$paneIndex % $window.tabs.Count]
        $paneId = "pane-extra-$paneIndex"
        $tab.panes += [ordered]@{ id = $paneId }
    }
    return [ordered]@{ schemaVersion = 2; windows = @($windows) }
}

function Measure-AverageMilliseconds
{
    param(
        [Parameter(Mandatory)]
        [scriptblock]$Operation,

        [Parameter(Mandatory)]
        [int]$Count
    )

    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    for ($index = 0; $index -lt $Count; $index++)
    {
        & $Operation
    }
    $watch.Stop()
    return [math]::Round($watch.Elapsed.TotalMilliseconds / $Count, 4)
}

try
{
    $scenarios = @(
        @{ Name = '1 pane'; Windows = 1; Tabs = 1; Panes = 1 },
        @{ Name = '20 panes'; Windows = 1; Tabs = 1; Panes = 20 },
        @{ Name = '100 tabs'; Windows = 1; Tabs = 100; Panes = 100 },
        @{ Name = '4 windows'; Windows = 4; Tabs = 40; Panes = 40 }
    )
    $results = foreach ($scenario in $scenarios)
    {
        $model = New-WorkspaceModel -WindowCount $scenario.Windows -TabCount $scenario.Tabs -PaneCount $scenario.Panes
        $serialized = $model | ConvertTo-Json -Depth 64 -Compress
        [gc]::Collect()
        $memoryBefore = [gc]::GetTotalMemory($true)

        $dragStart = Measure-AverageMilliseconds -Count $Iterations -Operation {
            $source = [ordered]@{
                type = 'pane'
                windowId = 'window-0'
                tabId = 'tab-0'
                paneId = 'pane-0-0'
            }
            if ($source.paneId.Length -eq 0) { throw 'Invalid source.' }
        }
        $overlayUpdate = Measure-AverageMilliseconds -Count $Iterations -Operation {
            $zones = @('topLeft', 'top', 'topRight', 'left', 'center', 'right', 'bottomLeft', 'bottom', 'bottomRight')
            $enabled = @($zones | Where-Object { $_ -ne 'center' -or $scenario.Tabs -gt 0 })
            if ($enabled.Count -eq 0) { throw 'No overlay zones.' }
        }
        $targetSwitch = Measure-AverageMilliseconds -Count $Iterations -Operation {
            $targets = @(
                [pscustomobject]@{ Name = 'window'; Priority = 3 },
                [pscustomobject]@{ Name = 'pane'; Priority = 2 },
                [pscustomobject]@{ Name = 'slot'; Priority = 1 },
                [pscustomobject]@{ Name = 'strip'; Priority = 0 }
            )
            $selected = $targets | Sort-Object Priority | Select-Object -First 1
            if ($selected.Name -ne 'strip') { throw 'Target priority changed.' }
        }
        $dropCommit = Measure-AverageMilliseconds -Count $Iterations -Operation {
            $snapshot = $serialized | ConvertFrom-Json
            $targetTab = $snapshot.windows[0].tabs[0]
            $targetTab.layout = [ordered]@{
                type = 'split'
                orientation = 'vertical'
                ratio = 0.5
                first = New-PaneNode -Id 'benchmark-source'
                second = $targetTab.layout
            }
        }
        $rollback = Measure-AverageMilliseconds -Count $Iterations -Operation {
            $before = $serialized | ConvertFrom-Json
            $after = $serialized | ConvertFrom-Json
            $after = $before
            if ($after.schemaVersion -ne 2) { throw 'Rollback snapshot failed.' }
        }

        [gc]::Collect()
        $memoryAfter = [gc]::GetTotalMemory($true)
        [pscustomobject]@{
            Scenario = $scenario.Name
            DragStartLatencyMs = $dragStart
            OverlayUpdateMs = $overlayUpdate
            TargetSwitchLatencyMs = $targetSwitch
            DropCommitDurationMs = $dropCommit
            RollbackDurationMs = $rollback
            ManagedMemoryDeltaBytes = $memoryAfter - $memoryBefore
            Iterations = $Iterations
            MeasurementBoundary = 'PowerShell model harness; not runtime UI'
        }
    }
    $results | Format-Table -AutoSize
    Write-Host 'PASS: docking model benchmark completed without filesystem writes or live sessions.' -ForegroundColor Green
    Write-Host 'NOTE: These measurements are not XAML frame, ConPTY transfer, or packaged-app performance results.' -ForegroundColor Yellow
}
catch
{
    Write-Error "Docking benchmark failed: $($_.Exception.Message)"
    exit 1
}
