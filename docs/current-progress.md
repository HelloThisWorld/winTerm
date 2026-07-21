# Current Development Progress

Last updated: 2026-07-21

## Baseline

- Branch: `main`
- Baseline commit: `312b3da728c61fbf3afdc31ee9c0cc5e8de75883`
- Microsoft Terminal upstream revision: `1cea42d433253d95c4487a3037db48197b5e72f4`
- Application version: `0.7.0-beta.1`
- Supported release target: Windows 11 x64

## Completed in the working tree

- Verified the directed split and pane-control implementation boundaries.
- Updated winTerm publisher and executable metadata for the independent product.
- Added an unpackaged, self-contained runtime staging pipeline.
- Added Inno Setup and Portable ZIP packaging and validation.
- Added installer upgrade, launch, profile, data-preservation, and uninstall tests.
- Added Portable isolation and launch tests.
- Replaced the primary MSIX release flow with exact-allowlist Setup/Portable assets.
- Added release metadata, SHA-256 checksums, SPDX and CycloneDX SBOMs.
- Updated WinGet generation for the Inno installer.
- Updated build, installation, removal, branding, security, privacy, and release docs.
- Added strict retired-brand, secret, debug-output, identity-file, and asset scans.

## Local evidence

- Release x64 build: passed.
- Relevant compiled Settings Model, Terminal App, and Control tests: passed.
- Directed split and pane-control source checks: passed.
- Unpackaged GUI launch: passed.
- Portable extraction, isolated settings, and GUI launch: passed.
- Current-user installer install, repair/upgrade, PowerShell and Command Prompt
  launch, data preservation, uninstall, and Windows Terminal isolation: passed.
- Current-user default installation path, launch, registration, and uninstall: passed.
- Workflow syntax and source-boundary validation: passed.
- All-users custom and default-path installer tests: pending a clean elevated Windows runner.
- Installer signature status: explicitly unsigned.
- Public download and WinGet validation: pending a new, unused release version.

## Release boundary

The existing `v0.7.0-beta.1` tag and GitHub Release are immutable and must not be
overwritten. No new Release may be created or published until every required
clean-runner and public-download gate passes for a new version.
