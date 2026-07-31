# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [ValidateRange(0, 10000)]
    [int]$DelayMilliseconds = 800
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Send-Osc
{
    param(
        [Parameter(Mandatory)]
        [string]$Payload
    )

    [Console]::Write("$([char]27)]$Payload$([char]7)")
    if ($DelayMilliseconds -gt 0)
    {
        Start-Sleep -Milliseconds $DelayMilliseconds
    }
}

try
{
    Write-Host 'Visual Progress fixture: determinate 50%'
    Send-Osc '9;4;1;50'
    Write-Host 'Visual Progress fixture: paused at 65%'
    Send-Osc '9;4;4;65'
    Write-Host 'Visual Progress fixture: error retaining 65%'
    Send-Osc '9;4;2;65'
    Write-Host 'Visual Progress fixture: indeterminate'
    Send-Osc '9;4;3'
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
    Send-Osc '133;D;1'
    Send-Osc '133;A'
    Send-Osc '9;4;0'
}
catch
{
    Write-Error "Visual Progress smoke fixture failed: $($_.Exception.Message)"
    exit 1
}
