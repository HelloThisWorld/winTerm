[CmdletBinding()]
param(
    [ValidateNotNullOrEmpty()]
    [string]$Remote = 'upstream',

    [ValidateNotNullOrEmpty()]
    [string]$TargetBranch = 'release-1.25',

    [switch]$CreateBranch,

    [string]$BranchName = "sync/upstream-$(Get-Date -Format 'yyyyMMdd')",

    [switch]$SkipFetch
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$gitSafeRepositoryRoot = $repositoryRoot.Replace('\', '/')

function Invoke-Git
{
    param(
        [Parameter(Mandatory)]
        [string[]]$Arguments
    )

    $output = @(& git -c "safe.directory=$gitSafeRepositoryRoot" -C $repositoryRoot @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0)
    {
        $detail = ($output | Out-String).Trim()
        if (-not $detail)
        {
            $detail = "git exited with code $exitCode"
        }
        throw "git $($Arguments -join ' ') failed: $detail"
    }

    return $output
}

try
{
    if (-not (Get-Command git -ErrorAction SilentlyContinue))
    {
        throw 'Git is required but was not found on PATH.'
    }

    $status = @(Invoke-Git -Arguments @('status', '--porcelain=v1'))
    if ($status.Count -gt 0)
    {
        throw 'The working tree is not clean. Commit or stash local changes before preparing an upstream sync.'
    }

    $remoteNames = @(Invoke-Git -Arguments @('remote'))
    if ($remoteNames -notcontains $Remote)
    {
        throw "Remote '$Remote' is missing. Add it with: git remote add $Remote https://github.com/microsoft/terminal.git"
    }

    $remoteUrl = ((Invoke-Git -Arguments @('remote', 'get-url', $Remote)) | Select-Object -First 1).Trim()
    Write-Host "Upstream remote: $Remote ($remoteUrl)"

    if ($SkipFetch)
    {
        Write-Host 'SKIP: Fetch was explicitly disabled.' -ForegroundColor Yellow
    }
    else
    {
        Write-Host "Fetching $Remote/$TargetBranch..."
        $null = Invoke-Git -Arguments @('fetch', $Remote, $TargetBranch)
    }

    $targetReference = "refs/remotes/$Remote/$TargetBranch"
    $targetCommit = ((Invoke-Git -Arguments @('rev-parse', "$targetReference^{commit}")) | Select-Object -First 1).Trim()
    $currentBranch = ((Invoke-Git -Arguments @('branch', '--show-current')) | Select-Object -First 1).Trim()
    $currentCommit = ((Invoke-Git -Arguments @('rev-parse', 'HEAD^{commit}')) | Select-Object -First 1).Trim()

    if (-not $currentBranch)
    {
        throw 'The repository is in detached HEAD state. Switch to a work branch before syncing upstream.'
    }

    if ($CreateBranch)
    {
        if (-not $BranchName)
        {
            throw '-BranchName must not be empty when -CreateBranch is used.'
        }

        $null = Invoke-Git -Arguments @('check-ref-format', '--branch', $BranchName)
        & git -c "safe.directory=$gitSafeRepositoryRoot" -C $repositoryRoot show-ref --verify --quiet "refs/heads/$BranchName"
        if ($LASTEXITCODE -eq 0)
        {
            throw "Local branch '$BranchName' already exists. Choose another name."
        }
        if ($LASTEXITCODE -ne 1)
        {
            throw "Unable to determine whether local branch '$BranchName' exists."
        }

        $null = Invoke-Git -Arguments @('switch', '-c', $BranchName)
        $currentBranch = $BranchName
        Write-Host "Created sync branch '$BranchName' from $currentCommit." -ForegroundColor Green
    }

    Write-Host ''
    Write-Host "Current branch: $currentBranch"
    Write-Host "Current commit: $currentCommit"
    Write-Host "Target commit:  $targetCommit ($Remote/$TargetBranch)"
    Write-Host ''
    Write-Host 'No integration was performed. Review upstream changes, then choose one strategy:'
    Write-Host "  git merge --no-ff $Remote/$TargetBranch"
    Write-Host "  git rebase $Remote/$TargetBranch"
    Write-Host 'Resolve conflicts locally, rerun winTerm validation, and commit the reviewed result.'
}
catch
{
    Write-Error "winTerm upstream sync preparation failed: $($_.Exception.Message)"
    exit 1
}
