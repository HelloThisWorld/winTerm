# winTerm

[![Validation](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-validation.yml/badge.svg)](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-validation.yml)
[![Windows build](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-full-build.yml/badge.svg)](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-full-build.yml)
[![Latest release](https://img.shields.io/github/v/release/HelloThisWorld/winTerm?display_name=tag&label=release)](https://github.com/HelloThisWorld/winTerm/releases/latest)

## Download the latest winTerm release

[**Open the latest release and download winTerm for x64**](https://github.com/HelloThisWorld/winTerm/releases/latest)

The latest Release page provides the Setup EXE, Portable ZIP, release notes,
and checksums together.

The installer is unsigned, so Windows may display Unknown Publisher or a
SmartScreen warning. Download only from the official release above and verify
the included `SHA256SUMS.txt` before running it.

winTerm is an independent open-source Windows 11 terminal published by
`helloThisWorld`. It is based on Microsoft Terminal source code but is not a
Microsoft product and is not affiliated with or endorsed by Microsoft. It does
not use Microsoft, Windows, or Windows Terminal logos.

## Distribution

The primary distribution is an unpackaged, self-contained Inno Setup EXE. A
Portable ZIP is also provided. For a version `<version>`, the only recommended
application downloads are:

- `winTerm-<version>-setup-x64.exe` — current-user or all-users installation;
- `winTerm-<version>-portable-x64.zip` — extract and run without installation.

The current source version is `1.1.1`. See the
[latest official Release](https://github.com/HelloThisWorld/winTerm/releases/latest)
for the complete published asset list and checksums.

If a release is unsigned, its notes say so explicitly. Windows may display an
Unknown Publisher or SmartScreen warning; verify `SHA256SUMS.txt` from the same
Release. No MSIX certificate, Developer Mode, Visual Studio, Windows SDK, or
`Add-AppxPackage` is required to install a release EXE.

See [installation guidance](docs/user/installation.md) and the
[1.1.1 release notes](docs/releases/1.1.1.md).

## Core features

- Top, Bottom, Left, and Right splits relative to the focused pane, retaining
  profile selection and transactional rollback;
- border-drag pane resizing with continuous updates, minimum-size constraints,
  and local snap points at quarters, thirds, and 50%;
- Alt-modified free resizing, stable snap hysteresis, exact resize undo/redo,
  and a one-command **Balance Panes** action;
- compact pane headers with a pane icon, title, real status text, and unified
  overflow menu without pane-repositioning affordances;
- a website-aligned dark native window shell, tab strip, pane surfaces, thin
  blue-grey dividers, and mint focus/resize feedback;
- configurable pane resizing and Application UI settings, keyboard resize
  commands, High Contrast-aware divider feedback, and screen-reader labels;
- independent `%LOCALAPPDATA%\winTerm` data and `winterm.exe` command
  registration without replacing Windows Terminal or `wt.exe`;
- PowerShell, CMD, dynamic WSL profile discovery, themes, private fonts,
  workspaces, snapshots, diagnostics, and multiline-paste protection.

Because Windows filenames are case-insensitive, the product-cased executable
`winTerm.exe` is also directly invokable as `winterm.exe`. The installer uses
an App Paths entry rather than globally editing `PATH`.

## Portable mode

Extract the complete Portable ZIP to a writable directory and run
`winTerm.exe`. The included `portable.marker` makes winTerm store settings,
themes, workspaces, snapshots, logs, and updates under the adjacent `data`
directory. Removing the marker switches data storage to
`%LOCALAPPDATA%\winTerm`.

Portable mode does not modify the registry, create shortcuts, register an
uninstaller, or require administrator access.

## Build and test

Use PowerShell 7 and the Microsoft Terminal toolchain described in
[build guidance](docs/build.md):

```powershell
.\scripts\winterm\build.ps1 -Configuration Release -Platform x64 -IncludeTests
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Release -Platform x64
.\scripts\winterm\build-unpackaged.ps1 -Configuration Release -Platform x64
.\scripts\winterm\build-installer.ps1 -Version 1.1.1 -Platform x64
.\scripts\winterm\build-portable.ps1 -Version 1.1.1 -Platform x64
```

The unpackaged generator uses an unsigned MSIX only as an upstream build
intermediate to produce the merged resource index. MSIX is not copied to the
stage, release allowlist, or primary user installation flow.

## Privacy and security

winTerm does not collect command text, terminal output, clipboard content,
workspace contents, working directories, or usage analytics. Crash upload is
off and opt-in. See [PRIVACY.md](PRIVACY.md), [SECURITY.md](SECURITY.md), and
[SUPPORT.md](SUPPORT.md).

## Code signing policy

Free code signing provided by SignPath.io, certificate by SignPath Foundation.

### Roles

- Authors, committers, and reviewers:
  [HelloThisWorld](https://github.com/HelloThisWorld)
- Approver:
  [HelloThisWorld](https://github.com/HelloThisWorld)

Official release artifacts are built from this public repository using
GitHub Actions.

Every release signing request requires manual approval.

See [PRIVACY.md](PRIVACY.md) for the winTerm privacy policy.

## License and upstream

winTerm preserves the Microsoft Terminal MIT license, copyrights, and
third-party notices. The pinned upstream baseline is
`release-1.25@1cea42d433253d95c4487a3037db48197b5e72f4`.

See [LICENSE](LICENSE), [NOTICE.md](NOTICE.md),
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), and
[upstream synchronization](docs/upstream-sync.md).
