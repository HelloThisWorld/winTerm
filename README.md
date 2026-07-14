# winTerm

[![CI](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-validation.yml/badge.svg)](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-validation.yml)
[![License](https://img.shields.io/github/license/HelloThisWorld/winTerm)](LICENSE)

winTerm is an independent open-source terminal application based on Microsoft Windows Terminal.

winTerm is not affiliated with or endorsed by Microsoft.

winTerm v0.3 is a development release for Shell Experience foundations. It keeps the mature Microsoft Terminal engine and adds an independent package identity, application-data boundary, user-visible branding, original placeholder artwork, reproducible wrappers, validation, CI, an upstream synchronization workflow, Appearance source foundations, and packaged PowerShell/CMD shell-integration assets.

## Current development scope

The foundation preserves upstream tabs, panes, multiple windows, command palette, settings UI, keyboard shortcuts, clipboard, search, scrollback, ANSI color, Unicode, CJK, emoji fallback, and font-dependent Powerline rendering. It uses the upstream profile generators for PowerShell 7, Windows PowerShell 5.1, Command Prompt, and installed WSL distributions.

v0.3 adds a conservative local-shell integration layer: a PowerShell 7/Windows PowerShell 5.1 module, CMD initialization assets, OSC 9;9 and OSC 133 marks reused from upstream, Safe `ll`, `la`, `which`, `touch`, and `open` mappings, a Windows API helper for CMD, diagnostics, and a non-persisting paste-risk analyser.

It does not add a full GNU compatibility layer, bundled GNU utilities or Clink, global PowerShell profile changes, CMD AutoRun changes, renderer changes, extended session restoration, workspaces, docking, AI features, cloud history, remote persistence, or command/clipboard telemetry.

## Prerequisites

- Windows 11 x64 for the v0.1 validation target
- Visual Studio 2022 with the components in `.vsconfig`
- Windows SDK 10.0.22621.0
- PowerShell 7 or later (`pwsh.exe`)
- Git and the repository-provided NuGet client

See [Build and test](docs/build.md) for the complete setup and current environment limitations.

## Build, test, and package

Run these commands from a PowerShell 7 prompt:

```powershell
.\scripts\winterm\build.ps1 -Configuration Debug -Platform x64
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Debug -Platform x64
.\scripts\winterm\test-shell-integration.ps1 -Shell All
.\scripts\winterm\build.ps1 -Configuration Release -Platform x64
.\scripts\winterm\package.ps1 -Platform x64
```

The fast source-only suite also supports Windows PowerShell 5.1:

```powershell
.\scripts\winterm\test.ps1 -Suite Smoke
```

The development package uses `Kaname.winTerm`, registers `winterm.exe`, and does not claim `wt.exe`. The package wrapper creates an unsigned MSIX; installation requires a locally trusted development certificate whose subject matches the manifest publisher. Certificates and private keys must never be committed.

## Supported shells

- PowerShell 7, when `pwsh.exe` is installed
- Windows PowerShell 5.1
- Command Prompt
- Dynamically discovered WSL distributions, when WSL is installed

winTerm does not bundle PowerShell, WSL, or a Linux distribution.

## Current limitations

- This checkout has passed source-level smoke validation, but the local environment used for the v0.1 work did not contain PowerShell 7, Visual Studio/MSBuild, or Windows SDK 10.0.22621.0. No local binary or MSIX was produced here.
- The current local environment has Windows PowerShell 5.1 and CMD, but not PowerShell 7, Visual Studio/MSBuild, or the required SDK. Its execution policy also blocks script execution, so runtime module, build, package, launch, UI, clipboard, and rendering acceptance remain open.
- v0.2 Appearance remains source-level only: compilation, package payload, launch, Settings UI, Theme Gallery, font loading, and manual rendering have not been verified. Those facts are not treated as v0.3 passes.
- v0.3 packages the shell module, CMD assets, and helper source/project, but automatic profile launcher injection, Shell Experience settings UI, right-click behavior, paste confirmation UI, and built-app validation remain tracked acceptance work.
- Installation, launch, shell execution, tabs, panes, input, and rendering still require manual verification on a correctly provisioned Windows 11 x64 machine.
- The manifest publisher `CN=winTerm Development` is a development placeholder and must be replaced together with the signing certificate for a public release.
- `winterm.exe` is the packaged execution alias. A `winterm:` URI protocol is intentionally not registered in v0.1 because the upstream application has no matching URI activation handler to reuse safely.
- ARM64 is reserved by the wrappers but has not been validated for v0.1.

The evidence and outstanding checks are tracked in [v0.1 acceptance](docs/v0.1-acceptance.md).

The current baseline and v0.3 evidence are tracked in [v0.2 acceptance](docs/v0.2-acceptance.md) and [v0.3 acceptance](docs/v0.3-acceptance.md).

## Architecture and maintenance

- [Architecture and ownership](docs/architecture.md)
- [Brand and package identity](docs/branding.md)
- [Upstream synchronization](docs/upstream-sync.md)
- [Release process](docs/release-process.md)
- [Shell Experience architecture](docs/shell-experience-architecture.md)
- [Shell protocol](docs/shell-protocol.md)
- [PowerShell integration](docs/powershell-integration.md)
- [Command Prompt integration](docs/cmd-integration.md)
- [Linux command compatibility](docs/linux-compatibility.md)
- [Completion](docs/completion.md)
- [Clipboard and paste protection](docs/clipboard-and-paste-protection.md)
- [Shell diagnostics](docs/shell-diagnostics.md)

The source baseline is Microsoft Terminal `release-1.25` at commit `1cea42d433253d95c4487a3037db48197b5e72f4`. The `upstream` remote points to `https://github.com/microsoft/terminal.git`.

## License and attribution

winTerm is distributed under the repository [MIT License](LICENSE). It is based on the [Microsoft Terminal open-source project](https://github.com/microsoft/terminal), which is also MIT licensed. Microsoft copyright notices, [third-party notices](NOTICE.md), and package notices are retained.

Microsoft, Windows, and Windows Terminal are trademarks of Microsoft Corporation. Their names are used only for accurate attribution and technical compatibility documentation.
