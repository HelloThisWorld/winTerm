# Changelog

## 1.1.2 - 2026-07-24

### Fixed

- Pane divider visuals no longer draw ghost lines away from the actual split
  boundary. The visible divider previously used a primary-axis Center
  alignment with a leading-edge margin, which re-centered the line in the
  space remaining after the margin; it now shares the leading-edge coordinate
  system of its pointer target, so exactly one line renders at each logical
  split position and nested dividers stay inside their owning split node.
- Added source-level regression coverage that locks the visible divider, the
  pointer target, and the logical split position to one shared leading-edge
  alignment model and keeps divider visuals reattached across split, close,
  swap, restore, and workspace rebuild paths.

### Distribution

- Publishes the corrected pane-divider rendering as the verified x64 Setup EXE
  and Portable ZIP.
- Workspace schema 2, docking model 1, shell protocol 1, and theme schema 1
  are unchanged.
- The Setup EXE is not code-signed unless a trusted Authenticode certificate
  is configured at publication; verify it with `SHA256SUMS.txt`.

## 1.1.1 - 2026-07-24

### Fixed

- Added an explicit unsigned-installer disclosure to generated Release notes
  when no trusted Authenticode certificate is configured.
- Added release-workflow regression coverage for the signing disclosure.
- Updated the README and versioned documentation to point users to the
  v1.1.1 Release while retaining the stable `/releases/latest` download link.

### Distribution

- Publishes the native UI refresh and snapping pane-resize feature set prepared
  for 1.1.0 as the verified x64 Setup EXE and Portable ZIP.
- Supersedes the unpublished v1.1.0 Release attempt without moving or
  overwriting its tag.
- The Setup EXE is not code-signed; verify it with `SHA256SUMS.txt`.

## 1.1.0 - 2026-07-23

### Added

- Pane-border drag resizing with a separate 1-pixel visual divider and
  12-logical-pixel pointer target.
- Common-ratio snapping at 25%, one third, 50%, two thirds, and 75%, with an
  8-pixel entry threshold, 14-pixel release threshold, and Alt bypass.
- One-entry-per-commit pane resize history, exact undo/redo, Escape and capture
  loss rollback, and **Balance Panes**.
- Native settings for pane resizing, snap presets and custom ratios, advanced
  snap threshold, ratio indicator, Alt bypass, Application UI density, pane
  header visibility, profile icon, and active status.
- Centralized native design tokens and a website-aligned title bar, tab strip,
  pane header, divider, and terminal-shell palette.

### Removed

- Pane-header and pane-handle drag repositioning.
- Pane docking overlays, edge/corner/empty-slot move targets, pane drag
  sessions, movement leases, movement history, and keyboard move mode.
- winTerm pane move commands, command-line entry points, settings, menus, and
  active documentation. Top-level tab reordering remains unchanged.

### Changed

- Pane headers now use a pane icon that focuses the pane and exposes an accurate
  accessible name; the overflow button remains the primary pane-menu entry.
- Directed Top, Bottom, Left, and Right splitting remains relative to the
  focused pane.
- Workspace schema remains version 2 because final split ratios already
  serialize through the inherited split startup actions.
- Application, package, shell module, About, and release metadata now identify
  `1.1.0` and tag `v1.1.0`.

### Release status

- x64 installer and Portable ZIP remain the primary distributions.
- Native Release build, manual DPI/accessibility/rendering acceptance, package
  launch, and upgrade validation are required before publication.

## 1.0.0 - 2026-07-22

### Added

- Dedicated pane-handle focus and drag-threshold behavior with explicit failure and rollback states.
- Snap-style edge, corner, Empty Slot, new-tab, and new-window presentation models with accessible target names.
- Traditional unpackaged Inno Setup installer and Portable ZIP distributions with checksums, notices, and SPDX and CycloneDX SBOMs.
- Direct installer and Portable download links at the top of the README.

### Changed

- Promoted the displayed application and PowerShell module version to stable `1.0.0`.
- Advanced the monotonic Windows package-build version to `1.0.8.0`.
- Standardized the public GitHub release and tag on `v1.0.0`.

### Known limitations

- The installer is unsigned and Windows may show Unknown Publisher or SmartScreen.
- Cross-process live pane transfer and ARM64 distribution remain unsupported.
- Live pane-drag UI, Narrator, High Contrast, and mixed-DPI acceptance scenarios are documented but were not completed by the local model-test run.

> Historical note: pane repositioning described in the 1.0.0 entry was removed
> in 1.1.0 and is not available in current winTerm builds.

## 0.7.0-beta.5 - 2026-07-21

### Added

- Traditional unpackaged Inno Setup and Portable ZIP distributions.
- Exact release-asset allowlisting, SHA-256 checksums, release metadata, and SPDX and CycloneDX SBOMs.
- Installer and Portable launch, isolation, upgrade, data-preservation, and uninstall validation.

### Changed

- Unified winTerm-owned publisher metadata under `helloThisWorld`.
- Made the Setup EXE the recommended download and removed MSIX from the primary release asset set.
- Corrected GitHub Actions preflight exit-code handling after an expected missing-Release lookup and an empty legacy-brand scan.
- Made Windows Terminal isolation checks portable to Windows Server runners where the Appx module is unavailable.
- Kept Windows 11 x64 as the supported target while allowing the Windows Server 2022 GitHub runner to execute installer acceptance tests; Windows 10 remains rejected.
- Advanced the monotonic Windows package-build version to `1.0.7.0`.

### Release status

- This Beta is an unsigned Windows 11 x64 preview.
- Windows may show Unknown Publisher or SmartScreen warnings; users should verify `SHA256SUMS.txt`.
- ARM64 and cross-process live pane transfer remain unsupported.

## 0.7.0-beta.4 - 2026-07-21

### Changed

- Made Windows Terminal isolation checks portable to Windows Server runners where the Appx module is unavailable.
- Advanced the monotonic Windows package-build version to `1.0.6.0`.

### Release status

- The immutable tag exists, but no GitHub Release was published because the Windows 11-only installer correctly rejected the Windows Server 2022 runner before the acceptance test; the workflow stopped before creating a Draft Release.

## 0.7.0-beta.3 - 2026-07-21

### Changed

- Corrected GitHub Actions preflight exit-code handling after expected empty lookups.
- Advanced the monotonic Windows package-build version to `1.0.5.0`.

### Release status

- The immutable tag exists, but no GitHub Release was published because the Windows Server runner could not load the Appx module during the installer isolation test; the workflow stopped before creating a Draft Release.

## 0.7.0-beta.2 - 2026-07-21

### Added

- Traditional unpackaged Inno Setup and Portable ZIP distributions.
- Exact release-asset allowlisting, SHA-256 checksums, release metadata, and SPDX and CycloneDX SBOMs.
- Installer and Portable launch, isolation, upgrade, data-preservation, and uninstall validation.

### Changed

- Unified winTerm-owned publisher metadata under `helloThisWorld`.
- Made the Setup EXE the recommended download and removed MSIX from the primary release asset set.
- Advanced the monotonic Windows package-build version to `1.0.4.0`.

### Release status

- The immutable tag exists, but no GitHub Release was published because the release preflight correctly stopped before building or uploading assets.

## 0.7.0-beta.1 - 2026-07-21

### Added

- Directed split planning for Top, Bottom, Left, and Right relative to the focused pane.
- Compact pane-header, drag-handle, capability-aware pane-menu, and Snap-layout presentation models.
- Transaction-ready Move Pane to New Tab and Move Pane to New Window plans that retain the existing pane and session identity.
- Keyboard pane-move target cycling, accessible announcements, and configurable pane-control settings.
- Unit and source-boundary coverage for directed splits, pane menus, pane headers, handle input isolation, drag previews, moves, and keyboard movement.

### Changed

- Product metadata identifies `0.7.0-beta.1`; the monotonic Windows/MSIX package version is `1.0.3.0` so the Beta can upgrade an existing `1.0.2.0` installation.
- Same-tab pane dragging now uses the existing `LayoutTransformer` to remove, normalize, and reinsert the pane relative to the selected target.
- Pane movement previews are derived from the proposed layout and must validate before a drop can be requested.

### Release status

- `v0.7.0-beta.1` is published as a self-signed preview Release.
- The existing `v1.0.2` Stable workflow and Release remain isolated from this Beta tag.
- The remaining runtime and manual acceptance gaps are documented in `docs/v0.7-acceptance.md`; this Beta is not a Stable acceptance claim.

## 1.0.2 - Stable

### Fixed

- Reserved `Ctrl+C` for interrupting the foreground terminal process, including Python development servers and other long-running commands.
- Migrated the legacy generated `Ctrl+C` copy binding out of existing user settings while preserving `Ctrl+Shift+C` for copying selected text.
- Added regression coverage for the default right-click workflow: copy and clear an active selection, then paste when no selection remains.

### Documentation

- Added detailed keyboard, mouse, clipboard, paste-warning, and VT mouse-reporting instructions to the README.
- Updated clipboard documentation to match the integrated runtime behavior and its current safety boundaries.

### Security

- The Release continues to publish a self-signed x64 MSIX, public CER, installation instructions, checksums, notices, SBOMs, symbols, and provenance from the exact immutable `v1.0.2` commit.
- Existing `v1.0.1` assets are not replaced or modified.

### Known issues

- The installer is self-signed, is not publicly trusted or timestamped, and requires administrators to import the attached CER into Trusted People.
- ARM64 and Windows 10 are not supported by this Release.

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

## 0.6.0-beta.1 - Unpublished baseline

- Added public-beta release infrastructure, compatibility evidence, privacy boundaries, security reporting, Workspace and Docking source validation, and unsigned development packaging.
