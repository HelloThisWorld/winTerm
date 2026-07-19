# winTerm

[Download winTerm v1.0.2](https://github.com/HelloThisWorld/winTerm/releases/tag/v1.0.2)

[![Version](https://img.shields.io/badge/version-1.0.2-blue)](https://github.com/HelloThisWorld/winTerm/releases/tag/v1.0.2)
[![Validation](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-validation.yml/badge.svg)](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-validation.yml)
[![Windows build](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-full-build.yml/badge.svg)](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-full-build.yml)
[![Stable release](https://img.shields.io/badge/stable%20release-v1.0.2-success)](https://github.com/HelloThisWorld/winTerm/releases/latest)

winTerm is an independent open-source terminal application based on Microsoft Windows Terminal.

winTerm is not affiliated with or endorsed by Microsoft. It does not use Microsoft, Windows, or Windows Terminal logos.

## Latest stable release

Download the latest verified installer from the [GitHub Releases page](https://github.com/HelloThisWorld/winTerm/releases/latest). The v1.0.2 installer is self-signed rather than signed by a public certificate authority, so Windows does not trust it automatically. Download the `.msix`, matching `.cer`, `INSTALL.txt`, and `SHA256SUMS.txt` from the same Release, verify the hashes, then follow the included certificate installation steps.

## Supported Windows versions

The intended winTerm 1.0 support target is Windows 11 x64. Windows 10 is unsupported for the Stable commitment. ARM64 remains Disabled until a native ARM64 package is built, installed, and launched successfully and is actually attached to a public Release.

## Installation

The official installer must:

- be downloaded from the public `v1.0.2` Release in this repository;
- be named `winTerm-1.0.2-x64.msix`;
- match `SHA256SUMS.txt`;
- have a cryptographically valid signature matching the included `winTerm-1.0.2.cer`;
- install without Visual Studio, Git, Developer Mode, registry edits, global font installation, or profile modification.

Because the certificate is self-signed, an administrator must import it into the Local Machine `Trusted People` certificate store before installing the MSIX. See [installation guidance](docs/user/installation.md).

## Screenshots

Release screenshots must be captured from the signed winTerm package after branding and accessibility review. No Microsoft or Windows Terminal screenshot is reused as winTerm promotional artwork. Screenshots are therefore withheld while the Stable gate is blocked.

## Core features

- independent `HelloThisWorld.winTerm` package identity and `winterm.exe` alias;
- PowerShell 7, Windows PowerShell 5.1, CMD, and dynamic WSL profile foundations;
- conservative Linux-style Safe Compatibility for local PowerShell and CMD;
- built-in and open-source Themes with pinned hashes and license records;
- app-private Cascadia programming fonts;
- ANSI color, CJK, Emoji, multiple windows, tabs, panes, and Command Palette foundations inherited from Microsoft Terminal;
- Workspace Schema version 2, Named Workspaces, restore planning, backups, and crash-recovery foundations;
- transactional edge and corner Docking models, keyboard commands, and Layout Undo/Redo;
- multiline and suspicious-paste protection;
- user-generated redacted diagnostics.

Runtime evidence is tracked in [feature status](docs/feature-status.md) and the [compatibility matrix](docs/compatibility-matrix.md). Runtime Docking, cross-process Pane transfer, Update Check, Git Bash integration, and ARM64 are Disabled.

## Keyboard and mouse operations

winTerm keeps the standard terminal interrupt shortcut separate from clipboard operations:

- `Ctrl+C` sends an interrupt to the foreground terminal process. Use it to stop a Python development server, web server, build, or other running command.
- `Ctrl+Shift+C` copies the current terminal selection.
- `Ctrl+V` or `Ctrl+Shift+V` pastes clipboard text through winTerm's paste handling.

The default right-click workflow is designed for quick copy and paste:

1. Select text by holding the left mouse button and dragging across the terminal output.
2. Right-click once while the selection is active. winTerm copies the selected text to the clipboard and clears the selection.
3. Right-click again after the selection has cleared. Because there is no active selection, winTerm pastes the current clipboard text at the terminal cursor.

Multiline or large clipboard content may show a confirmation before it is sent to the shell. Cancelling that confirmation sends no input.

This workflow uses the default settings `copyOnSelect: false` and `rightClickContextMenu: false`. Enabling `copyOnSelect` copies immediately when the selection is completed. Enabling `rightClickContextMenu` changes right-click to open a context menu instead of performing the copy/paste workflow.

Full-screen terminal applications such as editors, multiplexers, and text user interfaces can enable VT mouse reporting. In that mode, mouse actions are sent to the running application. Hold `Shift` while selecting or right-clicking to temporarily use winTerm's local mouse handling.

## Privacy

winTerm does not collect command text, terminal output, clipboard content, Workspace contents, working directories, or usage analytics. Crash upload is off and opt-in. Update checks are disabled until explicit consent exists. See [PRIVACY.md](PRIVACY.md).

## Release status

The exact gate status is recorded in:

- [winTerm 1.0 release checklist](docs/release-checklist-v1.0.md);
- [current progress](docs/v1.0-progress.md);
- [security review](docs/security-review-v1.0.md);
- [accessibility audit](docs/accessibility-audit-v1.0.md);
- [performance validation](docs/performance-v1.0.md);
- [release notes](docs/releases/1.0.2.md).

GitHub Actions builds from the exact immutable `v1.0.2` Tag, creates a temporary non-exportable self-signed key, verifies the package identity and embedded signature, uploads only allowlisted assets, re-downloads them for verification, and then publishes the Release. It never replaces an existing Release asset.

## Build

Use the Microsoft Terminal upstream toolchain described in [build guidance](docs/build.md). A release build requires PowerShell 7, the supported Visual Studio toolset, Windows SDK 10.0.22621.0, and the repository’s pinned dependencies.

```powershell
.\scripts\winterm\build.ps1 -Configuration Release -Platform x64 -IncludeTests
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Release -Platform x64
```

To build an installable x64 package without a public CA, run the local
development wrapper from PowerShell 7:

```powershell
.\scripts\winterm\build-local-development.ps1 -IncludeTests
```

It builds and validates Release, creates a disposable self-signed code-signing
certificate, signs the MSIX, exports only the public `.cer`, verifies the
package signature and identity, and writes `SHA256SUMS.txt` under
`artifacts/local/winTerm-1.0.2-development-x64`. The private key is removed
after signing.

Before installation, an administrator must import the included `.cer` into
the Local Machine `Trusted People` certificate store. This package is for
local or controlled internal testing; it is not publicly trusted, timestamped,
or a Stable release. See [build guidance](docs/build.md) for prerequisites,
output files, installation steps, and optional arguments.

## License and upstream

winTerm retains the Microsoft Terminal MIT license, copyright notices, and third-party notices. The pinned upstream baseline is `release-1.25@1cea42d433253d95c4487a3037db48197b5e72f4`.

See [LICENSE](LICENSE), [NOTICE.md](NOTICE.md), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), and [upstream synchronization](docs/upstream-sync.md).
