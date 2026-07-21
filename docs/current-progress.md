# Current Development Progress

Last updated: 2026-07-21

## Repository state

- Current branch: `main`
- Implementation commit: `eed15979ecbc223f474d6812744a2eb345efe0a3`
- Starting commit: `312b3da728c61fbf3afdc31ee9c0cc5e8de75883`
- Microsoft Terminal upstream revision: `1cea42d433253d95c4487a3037db48197b5e72f4`
- Application version: `0.7.0-beta.3`
- Supported release target: Windows 11 x64
- Uncommitted implementation changes: none

## Completed phases

- Phases 1-9: repository inspection, strict branding validation, publisher
  metadata, and directed split and pane-control implementation verification.
- Phases 10-12: unpackaged self-contained staging, Inno Setup, and Portable ZIP.
- Phase 13 (local scope): CurrentUser custom/default installs, launch, repair,
  upgrade, data preservation, uninstall, reinstall, and Portable isolation.
- Phases 14-16: guarded Release and WinGet workflows, source/artifact scanning,
  checksums, release metadata, and SPDX and CycloneDX SBOMs.
- Phase 17: local x64 candidate artifacts generated and verified.

## Files changed

- Product metadata and portable storage selection under `src/`.
- Unpackaged, installer, Portable, validation, and test scripts under
  `scripts/winterm/`.
- Inno Setup configuration under `packaging/inno/`.
- Release, WinGet, and full-build workflows under `.github/workflows/`.
- Build, installation, removal, branding, release, privacy, and security docs.

## Test evidence

Passed:

- Release x64 build.
- Relevant compiled Settings Model, Terminal App, and Control tests.
- Directed split and pane-control source checks.
- Unpackaged GUI launch.
- Portable extraction, isolated adjacent data, and GUI launch.
- CurrentUser custom install, default/PowerShell/Command Prompt launch,
  repair/upgrade, data preservation, uninstall, and Windows Terminal isolation.
- CurrentUser default installation path, registration, launch, and uninstall.
- Exact release-asset allowlist, SHA-256, metadata, SBOM, and package scans.
- PowerShell syntax, workflow source boundaries, and GitHub Actions syntax.
- Tracked and generated-artifact retired-brand checks: zero references.

Failed:

- No final local test remains failed.

Skipped or pending external evidence:

- AllUsers custom/default installs require a clean elevated Windows runner.
- Clean Windows 11 VM checks for ANSI True Color, Emoji, themes, fonts, WSL,
  Workspace restore, and interactive pane controls.
- Public-download installation and `winget validate` require a new public Release.
- ARM64 build and runtime validation.

## Distribution status

- Installer: x64 candidate built and verified; explicitly unsigned.
- Portable: x64 candidate built, extracted, launched, and isolated successfully.
- Release: `v0.7.0-beta.3` is the selected unused release version. The immutable
  `v0.7.0-beta.2` tag did not produce a GitHub Release because preflight stopped
  before any build or upload.
- Current phase: build, tag, GitHub workflow verification, and retirement of old
  GitHub Release entries.

## Next exact action

Build and test `v0.7.0-beta.3`, push its exact annotated tag, allow the guarded
GitHub workflow to create and verify the new Release, and only after that Release
is public remove the older GitHub Release entries.
