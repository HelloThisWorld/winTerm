# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [ValidateRange(0, 10000)]
    [int]$DelayMilliseconds = 800,

    [Parameter()]
    [ValidateRange(0, 10000)]
    [int]$SoakIterations = 0
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Wait-DemoStep
{
    if ($DelayMilliseconds -gt 0)
    {
        Start-Sleep -Milliseconds $DelayMilliseconds
    }
}

function Send-Osc
{
    param(
        [Parameter(Mandatory)]
        [string]$Payload
    )

    [Console]::Write("$([char]27)]$Payload$([char]7)")
    Wait-DemoStep
}

function Send-ControlSequence
{
    param(
        [Parameter(Mandatory)]
        [string]$Payload
    )

    [Console]::Write("$([char]27)$Payload")
    Wait-DemoStep
}

function Write-TransientFrame
{
    param(
        [Parameter(Mandatory)]
        [string]$Provider,

        [Parameter(Mandatory)]
        [string]$Frame,

        [Parameter()]
        [switch]$Quiet
    )

    if (-not $Quiet)
    {
        Write-Host "CLI provider fixture: $Provider (synthetic carriage-return frame)"
    }

    # Keep the transient record and its erase-line terminator in one bounded
    # write so replacement-preview testing exercises the immediate decision
    # path rather than depending on PTY callback coalescing.
    [Console]::Write("$Frame`r$([char]27)[2K")
    Wait-DemoStep
}

function Invoke-ProviderFixtures
{
    $fixtures = @(
        [pscustomobject]@{
            Provider = 'Docker Pull'
            Frame = 'demo-layer: Downloading 512B/1.0kB'
            Summary = 'demo-layer: Pull complete (synthetic summary; must remain visible)'
        },
        [pscustomobject]@{
            Provider = 'Docker BuildKit'
            Frame = '#7 [3/8] RUN synthetic-build-step'
            Summary = '#7 DONE 0.1s (synthetic summary; must remain visible)'
        },
        [pscustomobject]@{
            Provider = 'pip'
            Frame = 'sample_package.whl 50% 512kB/1.0MB 2.0MB/s eta 0:00:01'
            Summary = 'Successfully downloaded sample-package (synthetic summary; must remain visible)'
        },
        [pscustomobject]@{
            Provider = 'Git'
            Frame = 'Receiving objects:  50% (50/100), 512.00 KiB | 1.00 MiB/s'
            Summary = 'Receiving objects: 100% (100/100), done. (synthetic summary; must remain visible)'
        },
        [pscustomobject]@{
            Provider = 'curl'
            Frame = ' 50  1024k   50  512k    0     0  1024k      0  0:00:01  0:00:00  0:00:01 1024k'
            Summary = 'curl synthetic transfer complete (ordinary output; must remain visible)'
        },
        [pscustomobject]@{
            Provider = 'wget'
            Frame = 'sample.bin 50%[========>         ] 512K 1.00MB/s eta 1s'
            Summary = 'sample.bin saved (synthetic summary; must remain visible)'
        },
        [pscustomobject]@{
            Provider = 'npm'
            Frame = 'npm resolving dependencies 5/10'
            Summary = 'added 10 packages in 1s (synthetic summary; must remain visible)'
        },
        [pscustomobject]@{
            Provider = 'pnpm'
            Frame = 'pnpm Progress: resolved 10, reused 5, downloaded 3, added 2'
            Summary = 'Packages: +10 (synthetic summary; must remain visible)'
        },
        [pscustomobject]@{
            Provider = 'yarn'
            Frame = 'yarn Fetching packages... 5/10'
            Summary = 'Done in 1.00s. (synthetic summary; must remain visible)'
        },
        [pscustomobject]@{
            Provider = 'nvm'
            Frame = 'nvm Downloading node.js 50% (512/1024 kB)'
            Summary = 'nvm installation complete (synthetic summary; must remain visible)'
        },
        [pscustomobject]@{
            Provider = 'Maven'
            Frame = '[INFO] Progress (1): 512/1024 kB'
            Summary = '[INFO] BUILD SUCCESS (synthetic summary; must remain visible)'
        },
        [pscustomobject]@{
            Provider = 'Gradle'
            Frame = '<==========----> 75% EXECUTING [1s]'
            Summary = 'BUILD SUCCESSFUL in 1s (synthetic summary; must remain visible)'
        },
        [pscustomobject]@{
            Provider = 'Generic fallback'
            Frame = '42% (42/100) 1.0 MB/s ETA 00:01'
            Summary = 'ordinary log after generic progress (must remain visible)'
        }
    )

    foreach ($fixture in $fixtures)
    {
        if ($fixture.Provider -eq 'curl')
        {
            Write-Host '  % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current'
            Wait-DemoStep
        }
        elseif ($fixture.Provider -eq 'wget')
        {
            Write-Host "Saving to: 'sample.bin'"
            Wait-DemoStep
        }

        Write-TransientFrame -Provider $fixture.Provider -Frame $fixture.Frame
        Write-Host $fixture.Summary
        Wait-DemoStep
    }
}

function Invoke-BoundedSoak
{
    if ($SoakIterations -eq 0)
    {
        return
    }

    Write-Host "Visual Progress optional bounded soak: $SoakIterations iterations"
    for ($iteration = 0; $iteration -lt $SoakIterations; $iteration++)
    {
        $value = $iteration % 101
        Send-Osc "9;4;1;$value"

        if ((($iteration + 1) % 100) -eq 0)
        {
            Write-TransientFrame -Provider 'Generic soak sample' -Frame "$value% ($value/100) 1.0 MB/s ETA 00:01" -Quiet
        }
    }

    Send-Osc '9;4;0'
    Write-Host 'Visual Progress optional bounded soak complete; explicit progress cleared.'
}

try
{
    Write-Host @'
Visual Progress manual checks:
- For active/background behavior, run this script in two split panes and move focus between them.
- Only the active pane should emit sparks; inactive panes should keep independent, simplified progress.
- Minimize, switch tabs, or deactivate the window to verify animation pauses or simplifies.
- Rerun after disabling Windows animations to verify the static Reduced Motion fallback.
- Rerun with a Windows contrast theme to verify the solid High Contrast fallback.
- For real long-running processes (a FastAPI/uvicorn dev server, a Spring Boot service, a Node.js dev
  server, `tail -f`, or `kubectl port-forward`), the launch animation plays one traversal and then
  disappears; later log output must not bring it back, and stopping the process still shows the
  normal terminal result presentation.
- Real Maven/Gradle progress (for example a Spring Boot build) stays owned by the provider; once only
  the long-running application remains, the expired launch fallback must not reappear.
- For Alternate Screen applications (k9s, vim, top, htop), the launch animation appears at most once;
  entering, leaving, and re-entering the Alternate Screen, resizing, or switching tabs must not
  replay it, and exiting still produces the normal final state when available.
- A short command keeps its current behavior: completion supersedes the launch animation immediately.
'@

    foreach ($payload in @('9;4;1;0', '9;4;1;1', '9;4;1;50', '9;4;1;99', '9;4;1;100'))
    {
        $value = $payload.Split(';')[-1]
        Write-Host "Visual Progress fixture: determinate $value%"
        Send-Osc $payload
    }

    Write-Host 'Visual Progress fixture: real phase regression from 80% to 20%'
    Send-Osc '9;4;1;80'
    Send-Osc '9;4;1;20'

    Write-Host 'Visual Progress fixture: paused at 65%'
    Send-Osc '9;4;4;65'

    Write-Host 'Visual Progress fixture: indeterminate'
    Send-Osc '9;4;3'

    Write-Host 'Visual Progress fixture: error retaining 65%'
    Send-Osc '9;4;2;65'

    Write-Host 'Visual Progress fixture: explicit cancellation and clear'
    Send-Osc '9;4;0'

    Write-Host 'Visual Progress fixture: semantic command cancellation at next prompt'
    Send-Osc '133;B'
    Send-Osc '133;C'
    Send-Osc '133;A'

    Write-Host 'Visual Progress fixture: clear explicit progress'
    Send-Osc '9;4;0'

    Write-Host 'Visual Progress fixture: semantic command start'
    Send-Osc '133;B'
    Send-Osc '133;C'
    Write-Host 'Visual Progress fixture: semantic successful command finish'
    Send-Osc '133;D;0'
    Write-Host 'Visual Progress fixture: next prompt clears completion'
    Send-Osc '133;A'

    Write-Host 'Visual Progress fixture: semantic failed command finish'
    Send-Osc '133;B'
    Send-Osc '133;C'
    Send-Osc '133;D;1'
    Send-Osc '133;A'
    Send-Osc '9;4;0'

    Write-Host 'Visual Progress fixture: long-running command one-shot launch'
    Write-Host '  Expected: the launch animation plays one traversal, then the overlay disappears while output continues.'
    Send-Osc '133;B'
    Send-Osc '133;C'
    for ($tick = 1; $tick -le 4; $tick++)
    {
        Write-Host "synthetic long-running service output tick $tick (no progress overlay expected after the first traversal)"
        Wait-DemoStep
    }
    Write-Host 'Visual Progress fixture: the long-running command still reports its real result'
    Send-Osc '133;D;0'
    Send-Osc '133;A'

    Write-Host 'Visual Progress fixture: alternate screen during a long-running command'
    Write-Host '  Expected: entering, leaving, and re-entering the alternate screen does not replay the launch animation.'
    Send-Osc '133;B'
    Send-Osc '133;C'
    Send-ControlSequence '[?1049h'
    Write-Host 'synthetic alternate-screen application frame'
    Send-ControlSequence '[?1049l'
    Send-ControlSequence '[?1049h'
    Write-Host 'synthetic alternate-screen application frame after re-entry'
    Send-ControlSequence '[?1049l'
    Send-Osc '133;D;0'
    Send-Osc '133;A'

    Invoke-ProviderFixtures
    Invoke-BoundedSoak

    Write-Host 'Visual Progress smoke fixture complete. No files or external commands were used.' -ForegroundColor Green
}
catch
{
    Write-Error "Visual Progress smoke fixture failed: $($_.Exception.Message)"
    exit 1
}
