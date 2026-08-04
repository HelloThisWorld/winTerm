# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

function Invoke-WinTermPrompt
{
    [CmdletBinding()]
    param(
        # The caller's $? captured before any other command ran. The prompt
        # wrapper must supply this because its own statements (Get-Module,
        # the module-scoped call) reset $? to $true before this function
        # could read it.
        [object]$LastSuccess = $null
    )

    if ($null -eq $LastSuccess)
    {
        $LastSuccess = $?
    }
    $lastSuccess = [bool]$LastSuccess

    # The marks are embedded in the returned prompt string rather than written
    # as side effects. The console host writes a prompt function's console
    # output before it writes the returned text, so a side-effect 133;B would
    # land before the visible prompt and the command region would start at the
    # prompt text instead of at the user's input.
    $prefix = ''
    if ($script:WinTermHasPrompted)
    {
        $prefix += Get-WinTermOscSequence -Payload ('133;D;' + (Get-WinTermExitCode -LastSuccess $lastSuccess))
    }
    $prefix += Get-WinTermOscSequence -Payload '133;A'
    $prefix += Get-WinTermCurrentDirectorySequence

    try
    {
        $promptText = if ($null -ne $script:WinTermOriginalPrompt) { & $script:WinTermOriginalPrompt } else { 'PS> ' }
    }
    catch
    {
        $script:WinTermLastIntegrationError = 'The existing PowerShell prompt function failed.'
        $promptText = 'PS> '
    }

    $suffix = Get-WinTermOscSequence -Payload '133;B'
    $script:WinTermHasPrompted = $true
    return ($prefix + $promptText + $suffix)
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
        # $? must be captured before any other statement: every command the
        # wrapper runs (including Get-Module) would overwrite it and turn
        # every finished command into a false success report.
        $winTermLastSuccess = $?
        $module = Get-Module -Name 'winTerm.Shell'
        if ($null -ne $module)
        {
            return & $module { param($s) Invoke-WinTermPrompt -LastSuccess $s } $winTermLastSuccess
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
