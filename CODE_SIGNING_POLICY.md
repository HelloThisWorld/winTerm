# winTerm code signing policy

Free code signing provided by SignPath.io, certificate by SignPath Foundation.

This document is the canonical winTerm code signing policy. The website copy at
<https://winterm.dev/code-signing-policy> mirrors it; if they disagree, this
repository is authoritative.

## Current signing status

The latest public release, winTerm 1.1.3, is **not Authenticode-signed**. The
SignPath Foundation certificate has not been issued for this project, and no
signed winTerm binaries exist yet. Windows may display Unknown Publisher or a
SmartScreen warning for the current Setup EXE. Verify every download against
`SHA256SUMS.txt` from the same official
[GitHub Release](https://github.com/HelloThisWorld/winTerm/releases/latest).
This section changes only after a downloaded release artifact passes
Authenticode verification.

## Roles

- Authors and committers: [HelloThisWorld](https://github.com/HelloThisWorld)
- Reviewers: [HelloThisWorld](https://github.com/HelloThisWorld)
- Approvers: [HelloThisWorld](https://github.com/HelloThisWorld)

Changes from external contributors are accepted only through pull requests
reviewed by a maintainer. Review explicitly covers source code, CI
configuration, GitHub Actions workflows, build and packaging scripts,
dependency downloads, and signing-related configuration. See
[CONTRIBUTING.md](CONTRIBUTING.md).

## Build provenance

Official winTerm releases are built from this public repository by GitHub
Actions on GitHub-hosted runners, starting from an immutable release tag that
must match `src/winterm/Branding/version.json`. Release assets are published
with SHA-256 checksums, SBOMs, and GitHub artifact attestations, and every
Draft Release asset is re-downloaded and verified before publication. See
[.github/workflows/release.yml](.github/workflows/release.yml) and
[docs/release-process.md](docs/release-process.md).

## Release signing approval

Every release signing request requires manual approval by an approver listed
above. No artifact outside the official release pipeline is signed.

## Artifact scope and ownership

winTerm is based on the Microsoft Terminal open-source codebase (pinned
baseline `release-1.25@1cea42d433253d95c4487a3037db48197b5e72f4`, see
[docs/upstream-sync.md](docs/upstream-sync.md)). For signing purposes:

- **winTerm-owned binaries** built from `src/winterm-tools` and the branded
  installer (`winTerm.exe` launcher, `winterm-shim.exe`, the Inno Setup EXE)
  carry `ProductName` `winTerm` and version metadata derived from
  `src/winterm/Branding/version.json`.
- **Modified upstream outputs** (`WindowsTerminal.exe`, `OpenConsole.exe`,
  `Microsoft.Terminal.*.dll`, and related binaries) are compiled from this
  repository's source tree, which modifies Microsoft Terminal under its MIT
  license.
- **Third-party redistributables** (for example `Microsoft.UI.Xaml` and
  Visual C++ runtime components) come from their upstream packages and are
  not signed by this project.

## Privacy

See [PRIVACY.md](PRIVACY.md). winTerm does not collect command text, terminal
output, clipboard content, Workspace contents, working-directory paths, or
usage analytics.

## Installation and removal

[docs/user/installation.md](docs/user/installation.md) documents what the
installer changes on a system; [docs/user/uninstall.md](docs/user/uninstall.md)
documents standard removal, which does not touch Microsoft Windows Terminal,
`wt.exe`, WSL, PowerShell profiles, or global fonts.
