# Build and test winTerm

## Supported development target

The winTerm 1.0 release target is Windows 11 x64. The wrappers accept `ARM64`, but ARM64 remains Disabled until a native build, install, and launch validation passes.

## Required tools

Install Visual Studio 2022 Community, Professional, Enterprise, or Build Tools with the components declared in the repository `.vsconfig`. Important components include:

- MSBuild and Visual C++ x64/x86 tools
- Desktop development with C++
- Universal Windows Platform development and C++ support
- MSIX Packaging tools
- Windows App SDK support
- Windows SDK 10.0.22621.0
- ARM64 C++ and UWP components only when attempting ARM64

Also install PowerShell 7 or later and Git. The upstream build module requires PowerShell 7; Windows PowerShell 5.1 can run only the winTerm source-level smoke suite.

The repository contains its expected NuGet client at `dep/nuget/nuget.exe`. Package restore is delegated to the upstream build environment; do not vendor restored `packages` output.

The current baseline has no `.gitmodules` file. If a later upstream integration adds one, run:

```powershell
git submodule update --init --recursive
```

## Repository setup

The expected remotes are:

```text
origin    the winTerm repository
upstream  https://github.com/microsoft/terminal.git
```

The source baseline is `release-1.25` commit `1cea42d433253d95c4487a3037db48197b5e72f4`.

## x64 builds

Use PowerShell 7 from the repository root:

```powershell
.\scripts\winterm\build.ps1 -Configuration Debug -Platform x64
.\scripts\winterm\build.ps1 -Configuration Release -Platform x64
```

The wrapper checks PowerShell, Visual Studio discovery, Windows SDK 10.0.22621.0, NuGet, the solution, and the package project. It then initializes the official upstream environment and invokes its build function with `WindowsTerminalBranding=WinTerm`.

Build output remains in upstream `bin` and intermediate directories. MSIX output is under `src/cascadia/CascadiaPackage/AppPackages`; these generated paths are ignored by Git.

## Tests

```powershell
.\scripts\winterm\test.ps1 -Suite Smoke
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Debug -Platform x64
.\scripts\winterm\test.ps1 -Suite Full -Configuration Release -Platform x64
```

- `Smoke` parses winTerm PowerShell scripts and verifies source identity, manifest, assets, settings isolation, and profile foundations. It does not require compiled binaries.
- `Relevant` runs Smoke plus the upstream `unitSettingsModel`, `terminalApp`, and `unitControl` groups against an existing build.
- `Full` requests all upstream tests and is intentionally opt-in.
- Pass `-Build` to Relevant or Full to build before running compiled tests.

## Package

```powershell
.\scripts\winterm\package.ps1 -Platform x64
```

The wrapper builds Release with MSIX generation and signing disabled, locates the package, unpacks it with `makeappx.exe`, revalidates the embedded manifest, and reports path, architecture, signature status, and installation guidance. See [Release process](release-process.md) before signing or installing.

## Self-signed local development package

Use the following command when a public CA or production signing service is
not available and the package will be used only for local or controlled
internal testing:

```powershell
.\scripts\winterm\build-local-development.ps1 -IncludeTests
```

The wrapper:

1. verifies the 1.0.2 version and runs the source-level Smoke suite;
2. builds the x64 Release MSIX with signing disabled;
3. validates the generated package;
4. runs the Relevant compiled tests when `-IncludeTests` is present;
5. creates a one-year, non-exportable, self-signed development key for
   `CN=winTerm Development`;
6. signs the MSIX without a timestamp and exports only the public `.cer`;
7. validates the package identity, version, architecture, `winterm.exe`
   alias, absence of `wt.exe`, and the cryptographic PKCS#7 signature;
8. removes the private key and writes installation instructions and
   `SHA256SUMS.txt`.

The default output directory is ignored by Git:

```text
artifacts/local/winTerm-1.0.2-development-x64/
    INSTALL.txt
    SHA256SUMS.txt
    winTerm-1.0.2-development.cer
    winTerm-1.0.2-development-x64.msix
```

The wrapper refuses to overwrite an existing output directory. Use a new
directory when preserving an earlier build:

```powershell
.\scripts\winterm\build-local-development.ps1 `
  -OutputDirectory artifacts/local/winTerm-development-test-2
```

An existing unsigned MSIX can be validated and signed without compiling it
again:

```powershell
.\scripts\winterm\build-local-development.ps1 `
  -PackagePath .\path\to\CascadiaPackage_1.0.2.0_x64.msix `
  -OutputDirectory artifacts/local/winTerm-development-from-existing-package
```

Do not pass a previously signed package. The wrapper refuses to replace or
append a signature.

To install the result:

1. verify the files against `SHA256SUMS.txt`;
2. open `winTerm-1.0.2-development.cer`;
3. choose **Install Certificate**, **Local Machine**, and **Trusted People**,
   and approve the administrator prompt;
4. open `winTerm-1.0.2-development-x64.msix` and select **Install**.

Equivalent elevated PowerShell commands are included in `INSTALL.txt`.
Remove the development certificate after testing. Public v1.0.2 uses the same
self-signing trust model but generates a distinct certificate from the exact
release Tag; trust only the CER downloaded from that GitHub Release.

## CI runner

The validation and full-build workflows use GitHub-hosted Windows runners. The Stable workflow accepts only an exact `v1.0.2` Tag checkout. It creates a temporary non-exportable self-signed key, publishes only the public CER, deletes the private key after signing, and verifies the released MSIX against that CER.

## Known build issues in the implementation environment

On 2026-07-18, the available implementation machine did not contain PowerShell 7, Visual Studio or `vswhere.exe`, MSBuild, MakeAppx, SignTool, Winget, or Windows SDK 10.0.22621.0. Consequently, no v1.0 compiler, unit-test binary, application launch, package, signature, or installer result is claimed from that machine.

Install the declared prerequisites, reopen a PowerShell 7 prompt, and rerun Smoke, both x64 builds, Relevant tests, and packaging. Do not treat a source-only Smoke pass as a successful application build.
