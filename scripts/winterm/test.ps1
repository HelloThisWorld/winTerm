# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter()]
    [ValidateSet('Smoke', 'Relevant', 'Full')]
    [string]$Suite = 'Smoke',

    [Parameter()]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [Parameter()]
    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [Parameter()]
    [switch]$Build,

    [Parameter()]
    [ValidateRange(1, 60)]
    [int]$CompiledTestTimeoutMinutes = 20,

    [Parameter()]
    [string]$TestResultsDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Stop-CompiledTestProcessTree
{
    param(
        [Parameter(Mandatory)]
        [System.Diagnostics.Process]$Process,

        [Parameter(Mandatory)]
        [string]$Suite
    )

    if ($Process.HasExited)
    {
        return
    }

    Write-Warning "Terminating timed-out compiled test suite '$Suite' and its complete process tree (PID $($Process.Id))."
    try
    {
        $Process.Kill($true)
    }
    catch
    {
        Write-Warning "Process.Kill(entireProcessTree) failed for '$Suite': $($_.Exception.Message). Falling back to taskkill.exe."
        & taskkill.exe /PID $Process.Id /T /F | Out-Host
        if ($LASTEXITCODE -notin @(0, 128))
        {
            throw "Failed to terminate compiled test suite '$Suite' and its descendants (taskkill exit $LASTEXITCODE)."
        }
    }

    if (-not $Process.WaitForExit(30000))
    {
        throw "Compiled test suite '$Suite' did not exit within 30 seconds after process-tree termination."
    }
}

function Invoke-BoundedTaefSuite
{
    param(
        [Parameter(Mandatory)]
        [string]$Suite,

        [Parameter(Mandatory)]
        [string]$Configuration,

        [Parameter(Mandatory)]
        [string]$Executable,

        [Parameter(Mandatory)]
        [string]$TestBinary,

        [Parameter(Mandatory)]
        [string]$WorkingDirectory,

        [Parameter(Mandatory)]
        [string]$ResultsDirectory,

        [Parameter(Mandatory)]
        [ValidateRange(1, 60)]
        [int]$TimeoutMinutes
    )

    foreach ($requiredFile in @(
        @{ Path = $Executable; Description = 'TAEF runner' },
        @{ Path = $TestBinary; Description = 'compiled test binary' }
    ))
    {
        if (-not (Test-Path -LiteralPath $requiredFile.Path -PathType Leaf))
        {
            throw "$($requiredFile.Description) for suite '$Suite' was not found at '$($requiredFile.Path)'."
        }
    }

    New-Item -ItemType Directory -Path $ResultsDirectory -Force | Out-Null
    $stdoutPath = Join-Path $ResultsDirectory "$Suite.stdout.log"
    $stderrPath = Join-Path $ResultsDirectory "$Suite.stderr.log"
    $diagnosticPath = Join-Path $ResultsDirectory "$Suite.diagnostic.txt"
    $startTime = [DateTimeOffset]::UtcNow
    $endTime = $startTime
    $status = 'runner-error'
    $exitCode = $null
    $runnerError = $null
    $process = $null
    $processStarted = $false
    $stdoutTask = $null
    $stderrTask = $null
    $command = "`"$Executable`" `"$TestBinary`""

    Write-Host "Running compiled test suite '$Suite' with a $TimeoutMinutes-minute process timeout: $command"
    try
    {
        $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $Executable
        $startInfo.WorkingDirectory = $WorkingDirectory
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        $startInfo.ArgumentList.Add($TestBinary)

        $process = [System.Diagnostics.Process]::new()
        $process.StartInfo = $startInfo
        if (-not $process.Start())
        {
            throw "The TAEF runner for suite '$Suite' did not start."
        }
        $processStarted = $true

        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $timeoutMilliseconds = $TimeoutMinutes * 60 * 1000
        if (-not $process.WaitForExit($timeoutMilliseconds))
        {
            $status = 'timed-out'
            Stop-CompiledTestProcessTree -Process $process -Suite $Suite
        }
        else
        {
            # The parameterless call flushes asynchronous output after the
            # process handle has signaled completion.
            $process.WaitForExit()
            $exitCode = $process.ExitCode
            $status = if ($exitCode -eq 0) { 'passed' } else { 'failed' }
        }
    }
    catch
    {
        $runnerError = $_
        if ($processStarted -and -not $process.HasExited)
        {
            Stop-CompiledTestProcessTree -Process $process -Suite $Suite
        }
    }
    finally
    {
        $endTime = [DateTimeOffset]::UtcNow
        $stdout = if ($stdoutTask) { $stdoutTask.GetAwaiter().GetResult() } else { '' }
        $stderr = if ($stderrTask) { $stderrTask.GetAwaiter().GetResult() } else { '' }
        [IO.File]::WriteAllText($stdoutPath, $stdout)
        [IO.File]::WriteAllText($stderrPath, $stderr)

        if (-not [string]::IsNullOrEmpty($stdout))
        {
            Write-Host -NoNewline $stdout
        }
        if (-not [string]::IsNullOrEmpty($stderr))
        {
            Write-Host -ForegroundColor Yellow -NoNewline $stderr
        }

        $exitCodeText = if ($null -eq $exitCode) { 'not-available' } else { [string]$exitCode }
        @(
            "Configuration=$Configuration",
            "Suite=$Suite",
            "Executable=$Executable",
            "TestBinary=$TestBinary",
            "Command=$command",
            "StartTimeUtc=$($startTime.ToString('o'))",
            "EndTimeUtc=$($endTime.ToString('o'))",
            "TimeoutMinutes=$TimeoutMinutes",
            "Status=$status",
            "ExitCode=$exitCodeText"
        ) | Set-Content -LiteralPath $diagnosticPath -Encoding utf8

        if ($process)
        {
            $process.Dispose()
        }
    }

    if ($runnerError)
    {
        throw "Compiled test suite '$Suite' runner failed: $($runnerError.Exception.Message)"
    }
    if ($status -eq 'timed-out')
    {
        throw "Compiled test suite '$Suite' timed out after $TimeoutMinutes minutes. The complete process tree was terminated. Diagnostics: '$diagnosticPath'."
    }
    if ($exitCode -ne 0)
    {
        throw "Compiled test suite '$Suite' failed with exit code $exitCode. Diagnostics: '$diagnosticPath'."
    }
}

function Test-PowerShellSyntax
{
    param(
        [Parameter(Mandatory)]
        [string]$Directory
    )

    foreach ($scriptFile in Get-ChildItem -LiteralPath $Directory -Recurse -File -Filter '*.ps1')
    {
        $tokens = $null
        $parseErrors = $null
        [System.Management.Automation.Language.Parser]::ParseFile($scriptFile.FullName, [ref]$tokens, [ref]$parseErrors) | Out-Null
        if ($parseErrors.Count -gt 0)
        {
            $details = $parseErrors | ForEach-Object { "line $($_.Extent.StartLineNumber): $($_.Message)" }
            throw "PowerShell syntax failed for '$($scriptFile.FullName)': $($details -join '; ')"
        }
        Write-Host "PASS: PowerShell syntax: $($scriptFile.Name)" -ForegroundColor Green
    }
}

function Test-ProfileFoundations
{
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot
    )

    $userDefaults = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalSettingsModel\userDefaults.json') -Raw
    $settingsSerialization = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalSettingsModel\CascadiaSettingsSerialization.cpp') -Raw
    $powerShellGenerator = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalSettingsModel\PowershellCoreProfileGenerator.cpp') -Raw
    $wslGenerator = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalSettingsModel\WslDistroGenerator.cpp') -Raw

    if (-not ($userDefaults.Contains('"name": "Windows PowerShell"') -and $userDefaults.Contains('WindowsPowerShell\\v1.0\\powershell.exe')))
    {
        throw 'Windows PowerShell 5.1 profile foundation is missing.'
    }
    Write-Host 'PASS: Windows PowerShell 5.1 profile foundation' -ForegroundColor Green

    if (-not ($userDefaults.Contains('%SystemRoot%\\System32\\cmd.exe') -and $settingsSerialization.Contains('CommandPromptDisplayName')))
    {
        throw 'Command Prompt profile foundation is missing.'
    }
    Write-Host 'PASS: Command Prompt profile foundation' -ForegroundColor Green

    if (-not ($powerShellGenerator.Contains('POWERSHELL_PREFERRED_PROFILE_NAME{ L"PowerShell" }') -and $settingsSerialization.Contains('preferredPowershellProfile')))
    {
        throw 'PowerShell 7 discovery or default preference is missing.'
    }
    Write-Host 'PASS: PowerShell 7 discovery and first-launch preference' -ForegroundColor Green

    if (-not ($wslGenerator.Contains('openWslRegKey()') -and $wslGenerator.Contains('if (wslRootKey)')))
    {
        throw 'WSL dynamic discovery or missing-WSL guard is missing.'
    }
    Write-Host 'PASS: WSL dynamic discovery and missing-WSL guard' -ForegroundColor Green
}

function Test-ShellExperienceFoundations
{
    param(
        [Parameter(Mandatory)]
        [string]$RepositoryRoot
    )

    $moduleManifest = Join-Path $RepositoryRoot 'shell\powershell\winTerm.Shell\winTerm.Shell.psd1'
    $moduleScript = Join-Path $RepositoryRoot 'shell\powershell\winTerm.Shell\winTerm.Shell.psm1'
    $cmdInit = Join-Path $RepositoryRoot 'shell\cmd\winterm-init.cmd'
    $protocol = Join-Path $RepositoryRoot 'src\winterm\Shell\Protocol\ShellIntegrationProtocol.cpp'

    foreach ($path in @($moduleManifest, $moduleScript, $cmdInit, $protocol))
    {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf))
        {
            throw "Shell Experience foundation is missing '$path'."
        }
    }

    $manifest = Import-PowerShellDataFile -LiteralPath $moduleManifest
    if ($manifest.ModuleVersion -ne '1.3.3' -or
        $manifest.PrivateData.PSData.Prerelease -ne '' -or
        $manifest.PowerShellVersion -ne '5.1')
    {
        throw 'The winTerm PowerShell module manifest does not declare the supported version boundary.'
    }

    $moduleContent = Get-Content -LiteralPath $moduleScript -Raw
    $cmdContent = Get-Content -LiteralPath $cmdInit -Raw
    $protocolContent = Get-Content -LiteralPath $protocol -Raw
    if ($moduleContent -match 'ExecutionPolicy|Bypass' -or $cmdContent -match 'AutoRun' -or $protocolContent -notmatch 'MaximumShellProtocolPayloadLength')
    {
        throw 'Shell Experience safety boundaries are missing or contain a forbidden policy bypass.'
    }

    $shellScriptFiles = Get-ChildItem -LiteralPath (Split-Path -Parent $moduleScript) -Recurse -File -Include '*.ps1', '*.psm1'
    foreach ($shellScriptFile in $shellScriptFiles)
    {
        $tokens = $null
        $parseErrors = $null
        [System.Management.Automation.Language.Parser]::ParseFile($shellScriptFile.FullName, [ref]$tokens, [ref]$parseErrors) | Out-Null
        if ($parseErrors.Count -gt 0)
        {
            $details = $parseErrors | ForEach-Object { "line $($_.Extent.StartLineNumber): $($_.Message)" }
            throw "PowerShell syntax failed for '$($shellScriptFile.FullName)': $($details -join '; ')"
        }
    }

    & (Join-Path $PSScriptRoot 'package-shell-assets.ps1')
    if (-not $?)
    {
        throw 'Shell asset validation failed.'
    }

    & (Join-Path $PSScriptRoot 'test-paste-protection.ps1')
    if (-not $?)
    {
        throw 'Paste protection source validation failed.'
    }

    $userDefaults = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalSettingsModel\userDefaults.json') -Raw
    $inboxDefaults = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalSettingsModel\defaults.json') -Raw
    $actionMapSerialization = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalSettingsModel\ActionMapSerialization.cpp') -Raw
    $controlSettings = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalSettingsModel\MTSMSettings.h') -Raw
    $controlInteractivity = Get-Content -LiteralPath (Join-Path $RepositoryRoot 'src\cascadia\TerminalControl\ControlInteractivity.cpp') -Raw
    if ($userDefaults.Contains('{ "id": "Terminal.CopyToClipboard", "keys": "ctrl+c" }'))
    {
        throw 'Ctrl+C must not be assigned to Copy in winTerm user defaults.'
    }
    if (-not $inboxDefaults.Contains('{ "keys": "ctrl+shift+c", "id": "Terminal.CopyToClipboard" }'))
    {
        throw 'The Ctrl+Shift+C copy shortcut is missing from inbox defaults.'
    }
    if (-not ($actionMapSerialization.Contains('idJson == L"Terminal.CopyToClipboard"') -and
        $actionMapSerialization.Contains('keyJson->asString() == "ctrl+c"') -and
        $actionMapSerialization.Contains('_fixupsAppliedDuringLoad = true;')))
    {
        throw 'The legacy Ctrl+C copy-binding migration is missing.'
    }
    Write-Host 'PASS: Ctrl+C is reserved for terminal interrupt input' -ForegroundColor Green

    if (-not ($userDefaults.Contains('"copyOnSelect": false') -and
        $controlSettings.Contains('RightClickContextMenu, "rightClickContextMenu", false') -and
        $controlInteractivity.Contains('CopySelectionToClipboard(shiftEnabled') -and
        $controlInteractivity.Contains('_core->ClearSelection();') -and
        $controlInteractivity.Contains('RequestPasteTextFromClipboard();')))
    {
        throw 'The default right-click copy-then-paste workflow is incomplete.'
    }
    Write-Host 'PASS: Right-click copies a selection and then pastes without a selection' -ForegroundColor Green

    Write-Host 'PASS: Shell Experience source foundations' -ForegroundColor Green
}

function Test-WorkspaceFoundations
{
    $repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
    $settingsModel = Get-Content -LiteralPath (Join-Path $repositoryRoot 'src\cascadia\TerminalSettingsModel\MTSMSettings.h') -Raw
    if (-not ($settingsModel.Contains('WT_BRANDING_WINTERM') -and $settingsModel.Contains('FirstWindowPreference::PersistedLayout')))
    {
        throw 'winTerm does not default to the inherited safe persisted-layout startup path.'
    }

    foreach ($scriptName in @(
        'test-workspace-model.ps1',
        'test-workspace-restore.ps1',
        'test-workspace-recovery.ps1',
        'test-workspace-import.ps1'
    ))
    {
        & (Join-Path $PSScriptRoot $scriptName)
        if (-not $?)
        {
            throw "Workspace validation script '$scriptName' failed."
        }
    }
    Write-Host 'PASS: Workspace Restore source foundations' -ForegroundColor Green
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$originalLocation = Get-Location

try
{
    Write-Host "Running winTerm $Suite tests ($Configuration, $Platform)..."
    Test-PowerShellSyntax -Directory $PSScriptRoot

    & (Join-Path $PSScriptRoot 'verify-version.ps1')
    if (-not $?)
    {
        throw 'Version consistency validation failed.'
    }

    & (Join-Path $PSScriptRoot 'test-release-workflow.ps1')
    if (-not $?)
    {
        throw 'Release workflow validation failed.'
    }

    & (Join-Path $PSScriptRoot 'test-ci-classification.ps1')
    if (-not $?)
    {
        throw 'CI change classification validation failed.'
    }

    & (Join-Path $PSScriptRoot 'validate-assets.ps1')
    if (-not $?)
    {
        throw 'Appearance asset validation failed.'
    }

    & (Join-Path $PSScriptRoot 'generate-third-party-notices.ps1') -Check
    if (-not $?)
    {
        throw 'Third-party notice validation failed.'
    }

    & (Join-Path $PSScriptRoot 'verify-branding.ps1')
    if (-not $?)
    {
        throw 'Branding verification failed.'
    }

    Test-ProfileFoundations -RepositoryRoot $repositoryRoot
    Test-ShellExperienceFoundations -RepositoryRoot $repositoryRoot
    Test-WorkspaceFoundations

    & (Join-Path $PSScriptRoot 'test-pane-controls.ps1') -Configuration $Configuration -Platform $Platform -SourceOnly
    if (-not $?)
    {
        throw 'Directed split and pane control source validation failed.'
    }

    & (Join-Path $PSScriptRoot 'test-pane-resizing.ps1') -Configuration $Configuration -Platform $Platform -SourceOnly
    if (-not $?)
    {
        throw 'Pane resize source validation failed.'
    }

    & (Join-Path $PSScriptRoot 'test-visual-progress.ps1') -Configuration $Configuration -Platform $Platform -SourceOnly
    if (-not $?)
    {
        throw 'Visual Progress source validation failed.'
    }

    & (Join-Path $PSScriptRoot 'test-command-timeline.ps1')
    if (-not $?)
    {
        throw 'Command Timeline Phase 2 source validation failed.'
    }

    & (Join-Path $PSScriptRoot 'test-diagnostics.ps1')
    if (-not $?)
    {
        throw 'Diagnostic redaction source validation failed.'
    }

    & (Join-Path $PSScriptRoot 'test-privacy.ps1')
    if (-not $?)
    {
        throw 'Privacy validation failed.'
    }

    if ($Suite -eq 'Smoke')
    {
        Write-Host 'SKIP: Compiled unit tests are not part of the Smoke suite.' -ForegroundColor Yellow
        Write-Host 'winTerm Smoke tests passed.' -ForegroundColor Green
        return
    }

    if ($PSVersionTable.PSVersion.Major -lt 7)
    {
        throw 'PowerShell 7 or later is required for compiled upstream tests.'
    }

    if ($Build)
    {
        & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform -IncludeTests
        if (-not $?)
        {
            throw 'Build failed.'
        }
    }

    $binaryDirectory = Join-Path $repositoryRoot "bin\$Platform\$Configuration"
    if (-not (Test-Path -LiteralPath $binaryDirectory))
    {
        throw "Compiled test output '$binaryDirectory' was not found. Run build.ps1 first or pass -Build."
    }

    $expectedTestBinaries = @(
        'UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll',
        'UnitTests_TerminalApp\Terminal.App.Unit.Tests.dll',
        'UnitTests_Control\Control.Unit.Tests.dll'
    )
    foreach ($relativeBinary in $expectedTestBinaries)
    {
        $testBinary = Join-Path $binaryDirectory $relativeBinary
        if (-not (Test-Path -LiteralPath $testBinary -PathType Leaf))
        {
            throw "Compiled test binary '$testBinary' was not found. Build the solution with -IncludeTests."
        }
    }

    Set-Location $repositoryRoot
    Import-Module (Join-Path $repositoryRoot 'tools\OpenConsole.psm1') -Force
    Set-MsBuildDevEnvironment

    if ($Suite -eq 'Relevant')
    {
        $resultsDirectory = if ([string]::IsNullOrWhiteSpace($TestResultsDirectory))
        {
            Join-Path $repositoryRoot "artifacts\test-results\$Configuration"
        }
        elseif ([IO.Path]::IsPathRooted($TestResultsDirectory))
        {
            [IO.Path]::GetFullPath($TestResultsDirectory)
        }
        else
        {
            [IO.Path]::GetFullPath((Join-Path $repositoryRoot $TestResultsDirectory))
        }

        $centralTaef = Join-Path $repositoryRoot "packages\Microsoft.Taef.10.100.251104001\build\Binaries\$Platform\te.exe"
        $compiledSuites = [ordered]@{
            unitSettingsModel = @{
                Executable = Join-Path $binaryDirectory 'UnitTests_SettingsModel\te.exe'
                TestBinary = Join-Path $binaryDirectory 'UnitTests_SettingsModel\SettingsModel.Unit.Tests.dll'
            }
            terminalApp = @{
                Executable = $centralTaef
                TestBinary = Join-Path $binaryDirectory 'UnitTests_TerminalApp\Terminal.App.Unit.Tests.dll'
            }
            unitControl = @{
                Executable = $centralTaef
                TestBinary = Join-Path $binaryDirectory 'UnitTests_Control\Control.Unit.Tests.dll'
            }
        }

        foreach ($testName in $compiledSuites.Keys)
        {
            $compiledSuite = $compiledSuites[$testName]
            Invoke-BoundedTaefSuite `
                -Suite $testName `
                -Configuration $Configuration `
                -Executable $compiledSuite.Executable `
                -TestBinary $compiledSuite.TestBinary `
                -WorkingDirectory $repositoryRoot `
                -ResultsDirectory $resultsDirectory `
                -TimeoutMinutes $CompiledTestTimeoutMinutes
        }
    }
    else
    {
        Invoke-OpenConsoleTests -AllTests -Platform $Platform -Configuration $Configuration
        if (-not $?)
        {
            throw 'Full upstream tests failed.'
        }
    }

    Write-Host "winTerm $Suite tests passed." -ForegroundColor Green
}
catch
{
    Write-Error "winTerm tests failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    Set-Location $originalLocation
}
