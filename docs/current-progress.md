# Current Development Progress

Last updated: 2026-07-22

## Repository state

- Current branch: `main`
- Microsoft Terminal upstream revision: `1cea42d433253d95c4487a3037db48197b5e72f4`
- Application version: `1.0.0`
- Internal monotonic package version: `1.0.8.0`
- Release tag: `v1.0.0`
- Supported release target: Windows 11 x64

## Implemented foundations

- Unpackaged self-contained application staging, Inno Setup, and Portable ZIP
  distribution pipelines.
- Directed splits, compact pane headers, a unified pane menu, explicit
  pane-handle drag states, Snap-style docking models, rollback, session
  ownership leases, layout history, and Workspace Schema 2 persistence.
- Release allowlisting, checksums, release metadata, third-party notices, SPDX
  and CycloneDX SBOMs, branding validation, and GitHub release automation.
- Installer and Portable isolation, upgrade, data preservation, uninstall, and
  Windows Terminal isolation test scripts.

## Current evidence

Passed locally:

- Settings Model x64 Debug build.
- Native Terminal App library x64 Debug build.
- All 65 tests selected by `*WinTerm*`.
- Version, branding, release-workflow, source-boundary, and workspace-fixture
  checks.
- Case-insensitive retired-brand scan with no matches.

Known limitations:

- Live pane-drag UI, Narrator, High Contrast, mixed-DPI, and live
  session-identity acceptance scenarios were not completed in the local run.
- Cross-process and cross-elevation live pane transfer remains unsupported.
- ARM64 artifacts are not published.
- The x64 installer is distributed unsigned unless a production Authenticode
  certificate is configured.

## Distribution action

Build the x64 Release stage, installer, Portable ZIP, notices, SBOMs, release
notes, and checksums; verify the exact asset allowlist; then publish only those
files on the `v1.0.0` GitHub Release. The README points to the official installer
and Portable asset URLs.
