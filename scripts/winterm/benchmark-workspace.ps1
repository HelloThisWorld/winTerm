# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function New-BenchmarkWorkspace
{
    param(
        [Parameter(Mandatory)]
        [int]$WindowCount,

        [Parameter(Mandatory)]
        [int]$PaneCount
    )

    $windows = for ($windowIndex = 0; $windowIndex -lt $WindowCount; $windowIndex++)
    {
        $tabsForWindow = [math]::Floor($PaneCount / $WindowCount)
        if ($windowIndex -lt ($PaneCount % $WindowCount))
        {
            $tabsForWindow++
        }
        $tabs = for ($tabIndex = 0; $tabIndex -lt $tabsForWindow; $tabIndex++)
        {
            $paneId = "pane-$windowIndex-$tabIndex-0"
            [ordered]@{
                id = "tab-$windowIndex-$tabIndex"
                title = 'Benchmark'
                activePaneId = $paneId
                panes = @([ordered]@{
                    id = $paneId
                    profileId = '{benchmark-profile}'
                    profileNameFallback = 'PowerShell'
                    shellType = 'powershell'
                    workingDirectory = [ordered]@{ kind = 'unknown'; value = ''; source = 'unknown' }
                    session = [ordered]@{ wasCommandRunning = $false; endedUnexpectedly = $false }
                })
                layout = [ordered]@{ type = 'pane'; paneId = $paneId }
            }
        }
        [ordered]@{
            id = "window-$windowIndex"
            monitor = [ordered]@{ workArea = [ordered]@{ x = 0; y = 0; width = 1920; height = 1080 }; dpiX = 96; dpiY = 96 }
            bounds = [ordered]@{ x = 100; y = 80; width = 1200; height = 800 }
            normalizedBounds = [ordered]@{ x = 0.05; y = 0.07; width = 0.625; height = 0.74 }
            windowState = 'normal'
            activeTabId = "tab-$windowIndex-0"
            tabs = @($tabs)
        }
    }
    return [ordered]@{
        schemaVersion = 1
        id = 'workspace.benchmark'
        name = 'Benchmark'
        createdAt = '2026-07-15T00:00:00Z'
        updatedAt = '2026-07-15T00:00:00Z'
        source = 'runtime'
        applicationVersion = '0.4.0-dev'
        protocolVersion = 1
        activeWindowId = 'window-0'
        windows = @($windows)
    }
}

try
{
    $scenarios = @(
        @{ Name = '1 window / 1 pane'; Windows = 1; Tabs = 1; Panes = 1 },
        @{ Name = '1 window / 20 tabs'; Windows = 1; Tabs = 20; Panes = 20 },
        @{ Name = '4 windows / 40 panes'; Windows = 4; Tabs = 10; Panes = 40 },
        @{ Name = '8 windows / 100 panes'; Windows = 8; Panes = 100 }
    )
    $results = foreach ($scenario in $scenarios)
    {
        $captureWatch = [System.Diagnostics.Stopwatch]::StartNew()
        $workspace = New-BenchmarkWorkspace -WindowCount $scenario.Windows -PaneCount $scenario.Panes
        $captureWatch.Stop()

        $serializeWatch = [System.Diagnostics.Stopwatch]::StartNew()
        $json = $workspace | ConvertTo-Json -Depth 64 -Compress
        $serializeWatch.Stop()

        $restoreWatch = [System.Diagnostics.Stopwatch]::StartNew()
        $restored = $json | ConvertFrom-Json
        $observedPanes = 0
        foreach ($window in $restored.windows) { foreach ($tab in $window.tabs) { $observedPanes += @($tab.panes).Count } }
        $restoreWatch.Stop()

        [pscustomobject]@{
            Scenario = $scenario.Name
            WindowCount = @($restored.windows).Count
            TabCount = $scenario.Panes
            PaneCount = $observedPanes
            CaptureDurationMs = [math]::Round($captureWatch.Elapsed.TotalMilliseconds, 3)
            SerializationDurationMs = [math]::Round($serializeWatch.Elapsed.TotalMilliseconds, 3)
            RestoreDurationMs = [math]::Round($restoreWatch.Elapsed.TotalMilliseconds, 3)
            Result = if ($observedPanes -eq $scenario.Panes) { 'Passed' } else { 'Failed' }
        }
    }
    $results | Format-Table -AutoSize
    if ($results.Result -contains 'Failed')
    {
        throw 'A workspace benchmark scenario did not preserve its expected pane count.'
    }
}
catch
{
    Write-Error "Workspace benchmark failed: $($_.Exception.Message)"
    exit 1
}
