# winTerm

[![Validation](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-validation.yml/badge.svg)](https://github.com/HelloThisWorld/winTerm/actions/workflows/winterm-validation.yml)
[![License](https://img.shields.io/github/license/HelloThisWorld/winTerm)](LICENSE)

winTerm is an independent, open-source Windows terminal application based on Microsoft Windows Terminal. It is not affiliated with or endorsed by Microsoft.

## Public Beta status

The current target is **winTerm 0.6.0-beta.1**. It prepares a public beta with repeatable validation, release metadata, checksums, SBOM generation, privacy boundaries, security reporting, user documentation, and structured feedback templates.

This is not a public release yet. The [release candidate gate](docs/v0.6-rc-gate.md) records the remaining evidence before a draft release can be approved. In particular, x64 CI builds and unsigned development MSIX packaging are verified; clean install, upgrade, uninstall, accessibility, PowerShell 7, WSL, live Docking, and ARM64 native validation still require provisioned Windows testing.

## What is available

- Independent MSIX identity `Kaname.winTerm`, `winterm.exe` alias, and separate application-data boundary. `wt.exe` and Windows Terminal settings are untouched.
- PowerShell 7, Windows PowerShell 5.1, Command Prompt, and dynamically discovered WSL profile foundations.
- Theme and app-private font asset registries, ANSI/Unicode foundations, paste-risk analysis, and conservative local-shell helpers.
- Workspace schema v2, recovery snapshots, named Workspaces, layout validation, empty slots, layout history, and Docking transaction safety models.
- Privacy-safe release and diagnostics foundations: no general telemetry, no default command/output/clipboard collection, and opt-in-only future update or crash upload paths.

See the [compatibility matrix](docs/compatibility-matrix.md) for evidence by shell and architecture, and the [feature freeze](docs/v0.6-feature-freeze.md) for Stable/Beta/Experimental/Disabled classification.

## Important limitations

- The Visual Docking runtime adapter is **disabled by default**. The model, preview, rollback, and keyboard descriptions are present, but live transfer needs Windows runtime verification. Cross-process pane transfer is unsupported.
- Windows 11 x64 is the current CI packaging target. ARM64 is not supported until a native package is built and launched successfully.
- The development package is unsigned. Never treat it as a signed public release; verify official release SHA-256 checksums before installation.
- winTerm does not provide general Bash compatibility, remote session persistence, shell restarts to imitate session transfer, cloud synchronization, AI command generation, or a plugin marketplace.

## Install and get started

When an approved release is available, download only the official GitHub Release MSIX and verify `SHA256SUMS.txt` first. The development package uses `Kaname.winTerm` and can coexist with Windows Terminal.

- [Installation](docs/user/installation.md)
- [Getting started](docs/user/getting-started.md)
- [Shells and Linux compatibility](docs/user/shells.md)
- [Themes and fonts](docs/user/themes-and-fonts.md)
- [Workspaces](docs/user/workspaces.md)
- [Visual Docking](docs/user/visual-docking.md)
- [Keyboard shortcuts](docs/user/keyboard-shortcuts.md)
- [Accessibility](docs/user/accessibility.md)
- [Updates](docs/user/updates.md)
- [Diagnostics](docs/user/diagnostics.md)
- [Privacy](PRIVACY.md)
- [Uninstall](docs/user/uninstall.md)
- [Troubleshooting](docs/user/troubleshooting.md)

## Build, test, and package

Use a provisioned Windows development environment with Visual Studio 2022, Windows SDK 10.0.22621.0, PowerShell 7, Git, and the repository NuGet client.

```powershell
.\scripts\winterm\build.ps1 -Configuration Debug -Platform x64
.\scripts\winterm\test.ps1 -Suite Relevant -Configuration Debug -Platform x64
.\scripts\winterm\build.ps1 -Configuration Release -Platform x64
.\scripts\winterm\package.ps1 -Platform x64
```

The source-only smoke suite can run with Windows PowerShell 5.1:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\winterm\test.ps1 -Suite Smoke
```

Release automation builds x64 and ARM64 candidates, generates SHA-256 checksums and SPDX/CycloneDX metadata, and creates a **draft** GitHub Release. It never automatically publishes a release or stores signing secrets in the repository.

## Security and feedback

Use the [bug](.github/ISSUE_TEMPLATE/bug_report.yml), [compatibility](.github/ISSUE_TEMPLATE/compatibility_report.yml), or [crash](.github/ISSUE_TEMPLATE/crash_report.yml) template. Do not include passwords, tokens, private terminal output, proprietary source code, or unredacted Workspace files.

Report security vulnerabilities privately under the [security policy](SECURITY.md). See the [privacy policy](PRIVACY.md) before creating or sharing diagnostics.

## Release engineering

- [Feature freeze and classification](docs/v0.6-feature-freeze.md)
- [Performance baseline](docs/performance-baseline.md)
- [Accessibility audit](docs/accessibility-audit.md)
- [Security review](docs/security-review-v0.6.md)
- [Release checklist](docs/release-checklist.md)
- [Release notes draft](docs/releases/0.6.0-beta.1.md)
- [WinGet plan](packaging/winget/README.md)

The source baseline is Microsoft Terminal `release-1.25` at commit `1cea42d433253d95c4487a3037db48197b5e72f4`. The `upstream` remote points to `https://github.com/microsoft/terminal.git`.

## License and attribution

winTerm is distributed under the repository [MIT License](LICENSE). It is based on the [Microsoft Terminal open-source project](https://github.com/microsoft/terminal), which is also MIT licensed. Microsoft copyright notices, [third-party notices](NOTICE.md), and package notices are retained.

Microsoft, Windows, and Windows Terminal are trademarks of Microsoft Corporation. Their names are used only for accurate attribution and technical compatibility documentation.
