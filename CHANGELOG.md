# Changelog

## Unreleased

Fixes for the three alpha3 field reports.

### Fixed

- Opening a PowerShell tab no longer raises an antivirus alert. The `touch`
  compatibility command created files through a raw `File::Open` write call,
  which — combined with the native-dispatch blocks in the same script — read
  as a write-then-execute shape to antivirus heuristics and flagged the whole
  module at load, on some engines with a user-visible warning for every new
  tab. File creation now goes through `New-Item`; behavior is unchanged
  (create if missing, update the timestamp without truncating otherwise), and
  the module loads clean under the engine that previously flagged it.
- Completed commands now show ✓/✕ instead of staying `? Unknown`. The Enter
  keypress heuristic that supplies the command-executed transition only set
  buffer marks and never notified the shell-integration lifecycle, so the
  chain never observed the executed stage, the pane's capability never read
  Full, and the presentation downgraded every trusted result to Unknown. The
  Enter path now reports the executed transition when the mark was
  established by the shell and the input line is non-empty; a heuristic-only
  mark (no shell integration, for example cmd.exe) still reports nothing.
  This also restores the Running status while a command executes.
- A long Timeline command is now readable in full: every row carries a
  tooltip with the complete command text, wrapped, with no marquee animation.
- The `Unknown` status now explains itself: hovering it shows that the shell
  did not report a result for this command, and that success and failure are
  shown only when shell integration reports them.

## 1.3.0-alpha3 - 2026-08-04

Third alpha prerelease: fixes for the five alpha2 field reports. Like the
previous alphas, this is published only as a tagged GitHub prerelease for
local testing — GitHub Latest and the winTerm website continue to point at
v1.2.0, and this version is not submitted to WinGet.

### Fixed

- Shell integration no longer prints stray `\` characters before every
  prompt. The module's string terminator was written as two characters —
  in PowerShell single quotes `'\\'` is a literal double backslash — so the
  terminal consumed the well-formed sequence and printed the leftover
  backslash: three before the first prompt (A, cwd, B) and four after a
  command (D as well).
- The Command Timeline now records exactly the typed command. The prompt
  marks were written as console side effects while the prompt function ran,
  and the host writes a prompt function's console output before its returned
  text, so the command-start mark landed before the visible prompt and the
  captured "command" included the whole prompt line. The marks are now
  embedded in the returned prompt string in FinalTerm order, which also
  fixes Load inserting the prompt path into the input line.
- A shell-integrated pane no longer animates the Visual Progress bar while
  sitting at an idle prompt. `133;B` means the user is composing input, so
  it now hides the shell progress snapshot; only `133;C` (command executed)
  starts the running bar.
- The Timeline no longer shows a phantom `Command text unavailable` /
  `Running` entry for the active prompt. A `133;B`-only mark is the user
  composing input, not a command, and no longer creates an entry; the entry
  materializes when the command executes.
- Timeline rows no longer jump when moving the selection with the arrow
  keys or hovering with the mouse. Selection-only updates now reuse the
  existing rows instead of rebuilding every XAML element.
- A module component blocked by antivirus at parse time (observed for
  `Compatibility.ps1` under some engines) no longer spills a parse error
  into the session. The component is skipped and recorded in diagnostics;
  shell integration and the remaining commands still load.

### Changed

- Advanced the package/file version to `1.3.0.2`. The application version is
  `1.3.0-alpha3`, the PowerShell module stays `1.3.0` with prerelease suffix
  `alpha3`, and the channel stays `alpha`.

## 1.3.0-alpha2 - 2026-08-04

Second alpha prerelease: fixes for the four alpha1 field reports. Like alpha1,
this is published only as a tagged GitHub prerelease for local testing — GitHub
Latest and the winTerm website continue to point at v1.2.0, and this version is
not submitted to WinGet.

### Added

- Automatic PowerShell shell integration. When a profile's commandline is a
  bare `powershell.exe` or `pwsh.exe` invocation (optionally with `-NoLogo` or
  `-NoExit`), winTerm now appends a `-NoExit -Command` fragment that imports
  the packaged `winTerm.Shell` module, so OSC 133 marks — and therefore the
  Command Timeline — work out of the box for the stock Windows PowerShell
  profile. Any customized invocation launches unchanged, execution policy is
  never altered, and an import failure leaves a working shell. The new
  per-profile setting `"shellIntegration.autoInject"` (default `true`) turns
  the rewrite off. This fixes typed commands never appearing in the Command
  Timeline on a fresh install.

### Fixed

- The Command Timeline handle no longer covers terminal content. It now rests
  as a 6-pixel strip flush against the terminal's left edge, in the manner of
  an auto-hiding scrollbar, and widens to show its chevron on hover, keyboard
  focus, or while the overlay is open.
- Clicking the terminal area while the Command Timeline is open now
  light-dismisses the overlay; the click still reaches the terminal.
- The Visual Progress bar no longer keeps animating after ordinary output such
  as a `dir` listing. Recognition of Gradle-style output now requires
  per-record build-tool evidence instead of a bare product-name mention, an
  established provider claim no longer rematches arbitrary later records, a
  still-running provider bar structurally clears after two consecutive
  ordinary records, and slashed dates such as `2025/10/13` are no longer read
  as completed/total meters. Success and Error results still persist until a
  later publication replaces them.

### Changed

- Advanced the package/file version to `1.3.0.1` so the alpha2 binaries are
  distinguishable from alpha1 in `FILEVERSION`/`PRODUCTVERSION`. The
  application version is `1.3.0-alpha2`, the PowerShell module stays `1.3.0`
  with prerelease suffix `alpha2`, and the channel stays `alpha`.

## 1.3.0-alpha1 - 2026-08-04

First public prerelease of the Command Timeline. This is an alpha for local
testing, not a stable release: GitHub Latest and the winTerm website continue
to point at v1.2.0, and this version is not submitted to WinGet.

### Added

- Promoted the Command Timeline to a testable prerelease, combining engineering
  checkpoints v1.2.1 through v1.2.4: the pane-owned OSC 133 index, the overlay
  and deterministic navigation, load/copy/jump entry actions, and pane-local
  search with the two public settings.
- Published `commandTimeline.enabled` (default `true`) and
  `commandTimeline.historyLimit` (default `500`, range 50–5000) under
  Settings → Appearance → Command timeline.

### Changed

- Advanced the release channel to `alpha` with application version
  `1.3.0-alpha1`, package/file version `1.3.0.0`, and PowerShell module version
  `1.3.0` plus prerelease suffix `alpha1`. The release workflow marks any
  non-stable channel as a GitHub prerelease with `--latest=false`, so the
  public Latest download remains v1.2.0.
- Relaxed `verify-version.ps1` from a stable-only gate to a channel-aware one.
  It now accepts `stable`, `alpha`, and `beta`, and additionally enforces that
  the channel, the module prerelease suffix, and the application-version suffix
  agree, that the package version stays four-part numeric, and that the module
  version stays numeric.
- Added a prerelease guard to the WinGet workflow so an alpha or beta Release
  never generates a WinGet manifest.

### Checkpoint status

- Workspace schema, docking model, shell protocol, theme schema, update
  manifest schema, package identity, and signing policy are unchanged from
  v1.2.0. The Setup EXE remains unsigned; see the release notes.

## 1.2.4 - 2026-08-04

### Added

- Added pane-local Command Timeline search. `/` or Tab moves focus to the filter
  box, typing filters the current pane's commands, Up and Down walk the filtered
  results, and Enter loads the selected command without executing it. Neither
  the focus keys nor the query text ever reach the PTY.
- Added the `commandTimeline.enabled` and `commandTimeline.historyLimit` global
  settings, exposed under Settings → Appearance → Command timeline. Defaults are
  `true` and `500`; the history limit accepts 50 through 5000 per pane.
- Added four distinct empty states so an unsupported shell is never reported as
  simply having run no commands: waiting for shell integration, command timeline
  unavailable, no commands yet, and no matching commands.
- Added bounded per-pane history with oldest-first eviction, plus deterministic
  coverage for filtered navigation, wheel accumulation over the filtered
  projection, surrogate-safe query truncation, eviction, and a 5000-entry
  worst-case search.

### Changed

- Search is a literal, case-insensitive substring match over each pane's bounded
  in-memory command text only. There is no regex, no fuzzy matching, no output
  search, and no terminal-buffer rescan.
- Queries are capped at 256 UTF-16 code units and truncated without leaving a
  lone surrogate. A query is never persisted: closing the overlay releases the
  query, the filtered projection, and every materialized row.
- The filtered projection keeps stable `CommandId` identity. A command that
  still matches stays selected, a command that stops matching hands selection to
  the nearest surviving match, and a new command only takes the selection when it
  matches and the view was already following the latest command.
- Escape now clears a non-empty query first and only closes the overlay once the
  query is already empty.
- Lowering `commandTimeline.historyLimit` evicts oldest-first immediately on
  panes that already exist. Raising it never resurrects an evicted command, and
  sequence IDs are never reused.
- Disabling `commandTimeline.enabled` hides the left-side handle and closes an
  overlay that is already open; the toggle shortcut no longer opens it.
- Advanced engineering application and PowerShell module versions to `1.2.4`,
  package/file versions to `1.2.4.0`, and the intended checkpoint tag to
  `v1.2.4`; workspace, docking, shell, theme, update-manifest, package identity,
  and signing-policy versions remain unchanged.

### Checkpoint status

- `v1.2.4` is an engineering checkpoint for Command Timeline Phase 4, not a
  public GitHub Release. GitHub Latest and README public downloads remain on
  v1.2.0. There is still no persistent history, no output cache, and no
  telemetry.
- Builds on Command Timeline Phase 3, squash-merged to `main` as `5fd2172`
  through pull request #29.

## 1.2.3 - 2026-08-03

### Added

- Added Command Timeline Phase 3 entry actions. Enter and a single click load
  the selected command onto the focused pane's input line, Space jumps the
  viewport to that command's native mark, and Ctrl+C copies the selected
  command text while the Timeline owns the keyboard.
- Added a per-entry context menu with copy command, copy output, and jump to
  output. Every action resolves through the stable pane-scoped `CommandId`, so
  eviction, reflow, or a list rebuild between the right-click and the
  invocation can never retarget the action to a different command.
- Added a pure C++ `CommandTimelineActionModel` that decides load eligibility,
  tracks the loaded command, and advances an execution generation. A completion
  that belongs to a retired generation is detectable and discarded, which is
  what protects the loaded-input state from a late-completing command.
- Added on-demand output resolution. Output is read straight from the terminal
  buffer only when the user explicitly asks to copy it, and is never cached,
  indexed, or retained by the Timeline.

### Changed

- A Timeline load never executes. The payload is filtered for control codes
  only, the `CarriageReturnNewline` paste filter is deliberately not applied,
  no carriage return is ever appended, and the text is sent straight to this
  pane's connection, so the Windows clipboard is never read and input broadcast
  cannot forward the load to another pane.
- A multi-line command is refused outright when the shell has not enabled
  bracketed paste, because the embedded line breaks would otherwise be consumed
  as command submissions. With bracketed paste the command loads literally.
- A load above 1024 characters asks for confirmation before it is placed on the
  input line; Escape cancels the pending confirmation before it closes the
  overlay.
- Advanced engineering application and PowerShell module versions to `1.2.3`,
  package/file versions to `1.2.3.0`, and the intended checkpoint tag to
  `v1.2.3`; workspace, docking, shell, theme, update-manifest, package identity,
  and signing-policy versions remain unchanged.

### Checkpoint status

- `v1.2.3` is an engineering checkpoint for Command Timeline Phase 3, not a
  public GitHub Release. GitHub Latest and README public downloads remain on
  v1.2.0. Search, filtering, and the public Command Timeline settings remain
  reserved for Phase 4.

## 1.2.2 - 2026-08-02

### Added

- Added the pane-owned Command Timeline Phase 2 overlay with a compact
  left-side handle. The overlay is layered above the terminal surface and does
  not change pane dimensions, terminal rows or columns, swap-chain size, PTY
  size, or padding.
- Added a deterministic pure C++ navigation/presentation model that consumes
  the Phase 1 index, materializes only visible entries, restores stable command
  selection and visual slots, reconciles clear/eviction/reflow, and preserves
  older-history browsing when new commands arrive.
- Added keyboard-only Up, Down, Left, Right, and Escape navigation, hover and
  click selection, page-edge one-in/one-out behavior, and pane-local
  high-precision wheel/trackpad accumulation with complete-row settling.
- Added list/list-item selection semantics, localized accessible names and
  trustworthy status text, status glyphs that do not rely on color, themed
  High Contrast presentation, and a no-animation Reduced Motion-safe path.
- Added focused model, control source-boundary, settings, shortcut collision,
  user-override, pane isolation, cleanup, warm-access, and privacy coverage.

### Changed

- Remapped the canonical defaults to `Ctrl+Tab` for the focused pane's Command
  Timeline, `Ctrl+T` for the next tab, `Ctrl+Shift+T` for the previous tab, and
  `Ctrl+Alt+T` for a new tab. Explicit user key bindings retain precedence.
- Advanced engineering application and PowerShell module versions to `1.2.2`,
  package/file versions to `1.2.2.0`, and the intended checkpoint tag to
  `v1.2.2`; workspace, docking, shell, theme, update-manifest, package identity,
  and signing-policy versions remain unchanged.
- Established the canonical root changelog plus GitHub Wiki changelog and
  per-source-commit development ledger, together with permanent contributor,
  agent, and pull-request policy for keeping them synchronized.

### Fixed

- Replaced the README's stale `winterm-full-build.yml` Windows build badge with
  a Windows CI badge and link for the existing `winterm-validation.yml`
  workflow.

### Checkpoint status

- `v1.2.2` is an engineering checkpoint for Command Timeline Phase 2, not a
  public GitHub Release. GitHub Latest and README public downloads remain on
  v1.2.0. Command insertion, copy, paste, execution, output jumping, and search
  remain reserved for later phases.

## 1.2.1 - 2026-08-02

### Added

- Added the pane-owned Command Timeline Phase 1 data layer with stable
  pane-scoped command IDs, native mark identity, an incremental OSC 133
  lifecycle, trustworthy completion mapping, bounded command-text caching, and
  no command-output cache or persistence.
- Added cold bootstrap through native mark extents and warm access keyed by
  `markRevision`, so unchanged reads do not rescan the TextBuffer.
- Added clear, scrollback-eviction, reflow, and pane-close cleanup that retains
  surviving native identities without duplicate entries or cross-pane state.
- Added focused deterministic and native OSC component tests for lifecycle,
  identity, capability, privacy, reflow, pruning, warm access, and cleanup.

### Changed

- Added bounded TAEF process-tree cleanup for hanging compiled tests and
  label-gated CI classification so ordinary pull requests run quick validation
  without an expensive native build; `build`, `delivery`, and `ci:full` remain
  explicit maintainer-selected gates.

### Checkpoint status

- `v1.2.1` is an engineering checkpoint, not a public GitHub Release. It added
  no Timeline overlay, input behavior, installer, Latest, or website change.

## 1.2.0 - 2026-08-01

### Added

- Added stable per-pane Visual Progress with the Rainbow Arc Weld overlay for
  determinate, indeterminate, waiting, success, error, and cancelled states.
- Added bounded, local-only CLI progress recognition for Docker, BuildKit, pip,
  Git, curl, wget, npm, pnpm, yarn, nvm, Maven, Gradle, and a conservative
  generic fallback. Explicit OSC 9;4 progress retains highest precedence.
- Added **Settings → Appearance → Visual progress** controls. Visual Progress
  and CLI recognition default on, performance defaults to Automatic, and
  recognized-output replacement remains off by default.
- Added accessible ProgressBar semantics, real determinate values, nonnumeric
  indeterminate state, localized status text, and throttled active-pane-only
  announcements that never expose command, provider, or path content.

### Changed

- Added an adaptive rendering governor with Full, Balanced, and Minimal user
  modes; Automatic mode bounds effects by active progress count, UI dispatch
  latency, Reduced Motion, High Contrast, remote/software rendering, energy
  state, window visibility, and focus.
- Updated application, package, PowerShell module, About, executable resources,
  Workspace fallbacks, and release metadata to `1.2.0`, package `1.2.0.0`, with
  intended tag `v1.2.0`. Existing workspace, docking, shell, theme, and update
  schema/protocol versions remain unchanged.

### Distribution

- Prepares the x64 Setup EXE and Portable ZIP release flows with checksums,
  SPDX and CycloneDX SBOMs, third-party notices, and release metadata.
- The Setup EXE is not Authenticode-signed; Windows may display Unknown
  Publisher or SmartScreen, so download it from the official Release and verify
  it with `SHA256SUMS.txt`.

## 1.1.3 - 2026-07-26

### Changed

- Refreshed the native title bar, tab strip, pane headers, dividers, window
  controls, and terminal-shell palette to match the visual language used on
  winterm.dev while preserving existing terminal and pane behavior.
- Regenerated the complete Windows application, package, tile, ICO, and High
  Contrast artwork set from the canonical `assets/winterm/icons/winterm.svg`
  source.
- Updated application, package, PowerShell module, About, Workspace fallback,
  and release metadata to `1.1.3` with intended tag `v1.1.3`.

### Fixed

- Wrapped the tab strip and bottom border in a single XAML content container,
  resolving the duplicate `ContentPresenter.Content` assignment that blocked
  both Debug and Release builds.

### Distribution

- Publishes the website-aligned native shell as the verified x64 Setup EXE and
  Portable ZIP.
- Workspace schema 2, docking model 1, shell protocol 1, and theme schema 1
  remain unchanged.
- The Setup EXE is not Authenticode-signed; Windows may display Unknown
  Publisher or SmartScreen, so verify it with `SHA256SUMS.txt`.

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
