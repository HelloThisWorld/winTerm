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

## CI runner

The validation and full-build workflows use GitHub-hosted Windows runners. The Stable workflow uses the protected `winterm-stable-release` environment and an exact `v1.0.0` checkout. Only the protected prepare job may access production signing configuration; pull-request workflows never receive those secrets.

## Known build issues in the implementation environment

On 2026-07-18, the available implementation machine did not contain PowerShell 7, Visual Studio or `vswhere.exe`, MSBuild, MakeAppx, SignTool, Winget, or Windows SDK 10.0.22621.0. Consequently, no v1.0 compiler, unit-test binary, application launch, package, signature, or installer result is claimed from that machine.

Install the declared prerequisites, reopen a PowerShell 7 prompt, and rerun Smoke, both x64 builds, Relevant tests, and packaging. Do not treat a source-only Smoke pass as a successful application build.
