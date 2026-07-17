# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string[]]$Path,

    [Parameter(Mandatory)]
    [string]$CertificatePath,

    [Parameter(Mandatory)]
    [string]$CertificatePassword,

    [Parameter(Mandatory)]
    [ValidatePattern('^CN=')]
    [string]$ExpectedPublisher,

    [Parameter(Mandatory)]
    [ValidatePattern('^https?://')]
    [string]$TimestampUrl
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$certificate = $null

try
{
    $certificateFile = (Resolve-Path -LiteralPath $CertificatePath).Path
    $flags = [Security.Cryptography.X509Certificates.X509KeyStorageFlags]::EphemeralKeySet
    $certificate = [Security.Cryptography.X509Certificates.X509Certificate2]::new(
        $certificateFile,
        $CertificatePassword,
        $flags)

    if (-not $certificate.HasPrivateKey)
    {
        throw 'The signing certificate does not contain a private key.'
    }
    if ($certificate.Subject -cne $ExpectedPublisher)
    {
        throw 'The signing certificate subject does not match the protected package publisher.'
    }
    if ($certificate.NotAfter.ToUniversalTime() -le [DateTime]::UtcNow.AddDays(7))
    {
        throw 'The signing certificate is expired or expires within seven days.'
    }
    $ekuExtension = @($certificate.Extensions | Where-Object { $_.Oid.Value -eq '2.5.29.37' })
    if ($ekuExtension.Count -ne 1)
    {
        throw 'The signing certificate does not contain one enhanced key usage extension.'
    }
    $enhancedKeyUsage = [Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension]$ekuExtension[0]
    $codeSigningEku = @($enhancedKeyUsage.EnhancedKeyUsages | ForEach-Object { $_.Value })
    if ($codeSigningEku -notcontains '1.3.6.1.5.5.7.3.3')
    {
        throw 'The certificate is not valid for code signing.'
    }

    $signTool = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($null -eq $signTool)
    {
        throw 'signtool.exe was not found.'
    }

    foreach ($artifactPath in $Path)
    {
        $artifact = Get-Item -LiteralPath (Resolve-Path -LiteralPath $artifactPath).Path
        if ($artifact.Extension -notin @('.msix', '.msixbundle'))
        {
            throw "Unsupported signing target '$($artifact.Name)'."
        }
        if (($artifact.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0)
        {
            throw "Signing target '$($artifact.Name)' is a reparse point."
        }

        & $signTool.Source sign /fd SHA256 /td SHA256 /tr $TimestampUrl /f $certificateFile /p $CertificatePassword $artifact.FullName
        if ($LASTEXITCODE -ne 0)
        {
            throw "signtool.exe failed for '$($artifact.Name)' with exit code $LASTEXITCODE."
        }
        & $signTool.Source verify /pa /all /v $artifact.FullName
        if ($LASTEXITCODE -ne 0)
        {
            throw "signtool.exe trust verification failed for '$($artifact.Name)' with exit code $LASTEXITCODE."
        }

        $signature = Get-AuthenticodeSignature -LiteralPath $artifact.FullName
        if ($signature.Status -ne 'Valid' -or $null -eq $signature.SignerCertificate)
        {
            throw "Signature validation failed for '$($artifact.Name)': $($signature.Status)."
        }
        if ($signature.SignerCertificate.Subject -cne $ExpectedPublisher)
        {
            throw "The signer subject for '$($artifact.Name)' does not match the package publisher."
        }
        if ($null -eq $signature.TimeStamperCertificate)
        {
            throw "The signature for '$($artifact.Name)' does not contain a trusted timestamp."
        }
        Write-Host "PASS: signed and timestamped $($artifact.Name)" -ForegroundColor Green
    }
}
catch
{
    Write-Error "Release signing failed: $($_.Exception.Message)"
    exit 1
}
finally
{
    if ($null -ne $certificate)
    {
        $certificate.Dispose()
    }
}
