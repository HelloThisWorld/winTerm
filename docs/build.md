# Build, package, and test winTerm

## Supported target and tools

The supported release target is Windows 11 x64. ARM64 remains disabled until a
native build, install, launch, and release-asset validation succeeds.

Use PowerShell 7, Git, and Visual Studio 2022 with the components in `.vsconfig`.
The upstream toolchain requires MSBuild, Visual C++ desktop and UWP support,
MSIX packaging tools, and Windows SDK 10.0.22621.0. Inno Setup 6.7.3 is required
only to compile the traditional Setup EXE. Release CI downloads that exact
official version and verifies its SHA-256 and trusted signature.

The source baseline is Microsoft Terminal `release-1.25` commit
`1cea42d433253d95c4487a3037db48197b5e72f4`.

## Compile and test

Run from the repository root in PowerShell 7:

```powershell
.\scripts\winterm\build.ps1 -Configuration Debug -Platform x64
.\scripts\winterm\build.ps1 -Configuration Release -Platform x64 -IncludeTests
.\scripts\winterm\test.ps1 -Suite Smoke
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Release -Platform x64
```

`Smoke` validates scripts, branding, release boundaries, manifests, assets,
settings isolation, and feature source boundaries. `Relevant` also runs the
upstream Settings Model, TerminalApp, and Control test groups. `Full` requests
all upstream tests and is opt-in.

## Build the self-contained stage

```powershell
.\scripts\winterm\build-unpackaged.ps1 `
  -Configuration Release `
  -Platform x64
```

The output is `artifacts/stage/winTerm-x64`. It includes executables, runtime
DLLs, the merged unpackaged resource index, shell integrations, themes, fonts,
icons, licenses, notices, and `version.json`. It contains no package identity,
certificate, private key, test binary, debug symbol, build cache, or local
absolute path.

Microsoft Terminal's supported unpackaged generator consumes an unsigned MSIX
and the pinned Microsoft.UI.Xaml package as build intermediates. Neither is a
release asset or user installation requirement.

## Build Setup and Portable distributions

```powershell
.\scripts\winterm\build-installer.ps1 `
  -Version 0.7.0-beta.1 `
  -Platform x64 `
  -InnoCompiler 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'

.\scripts\winterm\build-portable.ps1 `
  -Version 0.7.0-beta.1 `
  -Platform x64 `
  -SkipBuild
```

Outputs:

```text
artifacts/release/winTerm-0.7.0-beta.1-setup-x64.exe
artifacts/release/winTerm-0.7.0-beta.1-portable-x64.zip
```

Without `-SigningCertificateThumbprint`, the installer is intentionally
unsigned and the script prints an explicit warning. With a trusted code-signing
certificate in the current user's certificate store, pass its thumbprint and a
trusted `-TimestampUrl`; the script signs and verifies the Setup EXE. Never
commit or package a PFX, private key, password, or development certificate.

## Installer tests

Static validation:

```powershell
.\scripts\winterm\test-installer.ps1 `
  -InstallerPath .\artifacts\release\winTerm-0.7.0-beta.1-setup-x64.exe `
  -Version 0.7.0-beta.1
```

Current-user install, custom path, launch, repair/upgrade, uninstall, reinstall,
registration, user-data preservation, and Windows Terminal isolation:

```powershell
.\scripts\winterm\test-installer.ps1 `
  -InstallerPath .\artifacts\release\winTerm-0.7.0-beta.1-setup-x64.exe `
  -Version 0.7.0-beta.1 `
  -RunSilentRoundTrip
```

Add `-RunAllUsersRoundTrip` from an elevated PowerShell process to run the same
all-users gate. Tests use unique temporary install paths and remove only those
verified paths. They never remove existing `%LOCALAPPDATA%\winTerm` data.

## Release assets

`generate-release-artifacts.ps1` copies only the Setup EXE, Portable ZIP,
third-party notices, versioned notes, release metadata, SPDX SBOM, and CycloneDX
SBOM, then generates `SHA256SUMS.txt`. `verify-release-assets.ps1` enforces the
exact allowlist and revalidates every hash and payload.
