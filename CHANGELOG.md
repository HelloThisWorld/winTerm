# Changelog

## 1.0.0 - Release candidate

### Added

- Stable compatibility and schema policies for Workspace 2, Docking 1, Shell Protocol 1, Theme 1, and Update Manifest 1.
- Version-consistency, privacy, checksum, package, signing, SBOM, Attestation, and downloaded-asset verification.
- A protected two-stage GitHub Draft and Stable publication workflow.
- Stable release notes, feature status, security, accessibility, performance, support, and release checklists.

### Changed

- Application, package, executable metadata, Workspace metadata, and PowerShell module versions are 1.0.0.
- The release channel is `stable`.
- Runtime Docking, cross-process Pane transfer, Update Check, Git Bash integration, and ARM64 remain Disabled until their gates pass.

### Security

- Release assets are allowlisted and must come from the exact `v1.0.0` commit.
- Published installers require a production signature, trusted timestamp, package Publisher match, checksum verification, and clean-machine validation evidence.
- winTerm performs no update request without explicit consent.
- The Release workflow never uses `--clobber` and never silently replaces assets.

### Known issues

- See [winTerm 1.0.0 release notes](docs/releases/1.0.0.md).
- Stable publication remains blocked until signing, clean installation, upgrade, uninstall, accessibility, runtime, and performance gates pass.

## 0.6.0-beta.1 - Unpublished baseline

- Added public-beta release infrastructure, compatibility evidence, privacy boundaries, security reporting, Workspace and Docking source validation, and unsigned development packaging.
