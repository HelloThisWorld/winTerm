# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^CN=')]
    [string]$Publisher
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifestPath = Join-Path $repositoryRoot 'src\cascadia\CascadiaPackage\Package-winTerm.appxmanifest'

try
{
    if ($Publisher -match '(?i)Microsoft' -or $Publisher -eq 'CN=winTerm Development')
    {
        throw 'A stable package publisher must be a production winTerm identity and must not use Microsoft or the development placeholder.'
    }

    [xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
    $namespace = [System.Xml.XmlNamespaceManager]::new($manifest.NameTable)
    $namespace.AddNamespace('f', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
    $identity = $manifest.SelectSingleNode('/f:Package/f:Identity', $namespace)
    if ($null -eq $identity -or $identity.Name -ne 'Kaname.winTerm' -or $identity.Version -ne '1.0.0.0')
    {
        throw 'The winTerm package identity or version is not the expected stable value.'
    }

    $identity.SetAttribute('Publisher', $Publisher)
    $settings = [System.Xml.XmlWriterSettings]::new()
    $settings.Indent = $true
    $settings.Encoding = [System.Text.UTF8Encoding]::new($false)
    $writer = [System.Xml.XmlWriter]::Create($manifestPath, $settings)
    try
    {
        $manifest.Save($writer)
    }
    finally
    {
        $writer.Dispose()
    }
    Write-Host 'Prepared the package manifest with the protected production publisher.' -ForegroundColor Green
}
catch
{
    Write-Error "Package publisher preparation failed: $($_.Exception.Message)"
    exit 1
}
