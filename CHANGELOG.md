# Changelog

## 1.0.1 - Stable

### Changed

- Replaced the previous package and WinGet identity with `HelloThisWorld.winTerm`.
- Updated application, package, shell module, About, Workspace metadata, and release versions to 1.0.1.
- Added a direct GitHub Release download link and v1.0.1 badge to the README.
- Changed the exact-tag Release workflow to build and cryptographically verify a self-signed x64 MSIX.

### Security

- The Release uploads the final MSIX, public CER, installation instructions, checksums, notices, SBOMs, symbols, and provenance from the exact immutable `v1.0.1` commit.
- The temporary signing key is non-exportable and removed after signing; no private key is published.
- Release assets are allowlisted, re-downloaded, and verified before and after publication.

### Known issues

- The installer is self-signed, is not publicly trusted or timestamped, and requires administrators to import the attached CER into Trusted People.
- ARM64 and Windows 10 are not supported by this Release.

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
