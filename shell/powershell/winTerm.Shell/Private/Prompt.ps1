# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

function Invoke-WinTermPrompt
{
    [CmdletBinding()]
    param()

    $lastSuccess = $?
    if ($script:WinTermHasPrompted)
    {
        Write-WinTermOsc -Payload ('133;D;' + (Get-WinTermExitCode -LastSuccess $lastSuccess))
    }

    Write-WinTermOsc -Payload '133;A'
    Send-WinTermCurrentDirectory

    try
    {
        $promptText = if ($null -ne $script:WinTermOriginalPrompt) { & $script:WinTermOriginalPrompt } else { 'PS> ' }
    }
    catch
    {
        $script:WinTermLastIntegrationError = 'The existing PowerShell prompt function failed.'
        $promptText = 'PS> '
    }

    Write-WinTermOsc -Payload '133;B'
    $script:WinTermHasPrompted = $true
    return $promptText
}

function Test-WinTermPromptWrapper
{
    [CmdletBinding()]
    param()

    if ($null -eq $script:WinTermPromptWrapper)
    {
        return $false
    }

    $prompt = Get-Command -Name prompt -CommandType Function -ErrorAction SilentlyContinue | Select-Object -First 1
    return $null -ne $prompt -and $prompt.ScriptBlock.ToString() -eq $script:WinTermPromptWrapper.ToString()
}

function Enable-WinTermShellIntegration
{
    [CmdletBinding()]
    param(
        [switch]$Force
    )

    if (-not $Force -and -not (Test-WinTermInteractiveSession))
    {
        $script:WinTermLastIntegrationError = 'The winTerm session marker or an interactive console host was not detected.'
        return $false
    }

    if (Test-WinTermPromptWrapper)
    {
        $script:WinTermIntegrationEnabled = $true
        $script:WinTermPromptWrapped = $true
        return $true
    }

    $originalPrompt = Get-Command -Name prompt -CommandType Function -ErrorAction SilentlyContinue | Select-Object -First 1
    $script:WinTermOriginalPrompt = if ($null -ne $originalPrompt) { $originalPrompt.ScriptBlock } else { { 'PS> ' } }
    $script:WinTermPromptWrapper = {
        $module = Get-Module -Name 'winTerm.Shell'
        if ($null -ne $module)
        {
            return & $module { Invoke-WinTermPrompt }
        }
        return 'PS> '
    }

    Set-Item -Path Function:\global:prompt -Value $script:WinTermPromptWrapper -Force
    $script:WinTermIntegrationEnabled = $true
    $script:WinTermPromptWrapped = $true
    $script:WinTermLastIntegrationError = $null
    return $true
}

function Disable-WinTermShellIntegration
{
    [CmdletBinding()]
    param()

    if ((Test-WinTermPromptWrapper) -and $null -ne $script:WinTermOriginalPrompt)
    {
        Set-Item -Path Function:\global:prompt -Value $script:WinTermOriginalPrompt -Force
    }

    $script:WinTermIntegrationEnabled = $false
    $script:WinTermPromptWrapped = $false
    $script:WinTermHasPrompted = $false
    return $true
}
