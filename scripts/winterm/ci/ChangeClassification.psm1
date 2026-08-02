# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

Set-StrictMode -Version Latest

function Test-WinTermDocumentationPath
{
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $normalized = $Path.Replace('\', '/')
    if ($normalized -match '^(docs|doc)/' -and $normalized -notmatch '^docs/releases/')
    {
        return $true
    }
    if ($normalized -match '^\.github/(ISSUE_TEMPLATE/|PULL_REQUEST_TEMPLATE(?:\.md|/))')
    {
        return $true
    }

    return $normalized -cin @(
        'AGENTS.md',
        'CHANGELOG.md',
        'CODE_OF_CONDUCT.md',
        'CODE_SIGNING_POLICY.md',
        'CONTRIBUTING.md',
        'PRIVACY.md',
        'README.md',
        'SECURITY.md',
        'SUPPORT.md'
    )
}

function Test-WinTermDeliveryPath
{
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $normalized = $Path.Replace('\', '/')
    if ($normalized -match '^\.github/(workflows|actions)/' -or
        $normalized -match '^packaging/' -or
        $normalized -match '^build/' -or
        $normalized -match '^\.nuget/' -or
        $normalized -match '^dep/(nuget|vcpkg-overlay-ports|vcpkg-overlay-triplets)/' -or
        $normalized -match '^docs/releases/' -or
        $normalized -match '^src/winterm/Branding/' -or
        $normalized -match '^assets/winterm/(fonts|themes)/manifest\.json$')
    {
        return $true
    }

    if ($normalized -match '(?i)(^|/)(CMakeLists\.txt|NuGet\.config|packages\.config|vcpkg\.json)$' -or
        $normalized -match '(?i)\.(sln|slnx|vcxproj|wapproj|props|targets|cmake|iss|appxmanifest|manifest|rc)$')
    {
        return $true
    }

    if ($normalized -cin @(
        '.vsconfig',
        'dirs',
        'THIRD_PARTY_NOTICES.md'
    ))
    {
        return $true
    }

    if ($normalized -match '^scripts/winterm/' -and $normalized -match '(?i)(build|package|installer|portable|unpackaged|release|sign|attest|checksum|third-party-notices|winget|version|branding)')
    {
        return $true
    }

    return $false
}

function Get-WinTermChangeClassification
{
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$ChangedFiles,

        [Parameter()]
        [AllowEmptyCollection()]
        [string[]]$Labels = @(),

        [Parameter()]
        [ValidateSet('auto', 'quick', 'build', 'delivery', 'fast', 'full')]
        [string]$Mode = 'auto'
    )

    $normalizedFiles = @($ChangedFiles | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object { $_.Trim().Replace('\', '/') } | Sort-Object -Unique)
    $normalizedLabels = @($Labels | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object { $_.Trim().ToLowerInvariant() })
    $reasons = [System.Collections.Generic.List[string]]::new()

    if ($Mode -in @('delivery', 'full'))
    {
        $changeClass = 'delivery'
        $reasons.Add("workflow_dispatch selected $Mode validation")
    }
    elseif ($Mode -in @('build', 'fast'))
    {
        $changeClass = 'build'
        $reasons.Add("workflow_dispatch selected $Mode validation")
    }
    elseif ($Mode -eq 'quick')
    {
        $changeClass = 'validation-only'
        $reasons.Add('workflow_dispatch selected quick validation')
    }
    elseif ($normalizedLabels -contains 'delivery' -or $normalizedLabels -contains 'ci:full')
    {
        $changeClass = 'delivery'
        $reasons.Add('a delivery label requested Release delivery plus Debug validation')
    }
    elseif ($normalizedLabels -contains 'build')
    {
        $changeClass = 'build'
        $reasons.Add('the build label requested Release delivery validation')
    }
    elseif ($normalizedFiles.Count -eq 0)
    {
        $changeClass = 'validation-only'
        $reasons.Add('no changed files were resolved and no native-build label is present')
    }
    else
    {
        $nonDocumentationFiles = @($normalizedFiles | Where-Object { -not (Test-WinTermDocumentationPath -Path $_) })
        if ($nonDocumentationFiles.Count -eq 0)
        {
            $changeClass = 'docs-only'
            $reasons.Add('every changed file is in the conservative documentation allowlist')
        }
        else
        {
            $changeClass = 'validation-only'
            $deliveryFiles = @($normalizedFiles | Where-Object { Test-WinTermDeliveryPath -Path $_ })
            if ($deliveryFiles.Count -gt 0)
            {
                $reasons.Add("delivery-sensitive paths changed without a native-build label: $($deliveryFiles -join ', ')")
            }
            else
            {
                $reasons.Add("non-documentation paths changed without a native-build label: $($nonDocumentationFiles -join ', ')")
            }
        }
    }

    [pscustomobject]@{
        ChangeClass = $changeClass
        RunReleaseDelivery = $changeClass -in @('build', 'delivery')
        RunDebugValidation = $changeClass -eq 'delivery'
        ChangedFiles = $normalizedFiles
        Reasons = @($reasons)
    }
}

Export-ModuleMember -Function Get-WinTermChangeClassification, Test-WinTermDeliveryPath, Test-WinTermDocumentationPath
