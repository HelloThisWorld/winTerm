# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

function Register-WinTermCompatibilityCompletion
{
    [CmdletBinding()]
    param()

    if ($null -eq (Get-Command -Name Register-ArgumentCompleter -ErrorAction SilentlyContinue))
    {
        $script:WinTermCompletionProvider = 'PowerShell native completion only'
        return
    }

    Register-ArgumentCompleter -CommandName which -ParameterName Name -ScriptBlock {
        param($commandName, $parameterName, $wordToComplete)

        Get-Command -Name ($wordToComplete + '*') -ErrorAction SilentlyContinue |
            Sort-Object Name -Unique |
            ForEach-Object {
                [System.Management.Automation.CompletionResult]::new($_.Name, $_.Name, 'ParameterValue', $_.CommandType.ToString())
            }
    }

    if ($null -ne (Get-Module -Name PSReadLine | Select-Object -First 1))
    {
        $script:WinTermCompletionProvider = 'PowerShell native completion with PSReadLine'
    }
}
