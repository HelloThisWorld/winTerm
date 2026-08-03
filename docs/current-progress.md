# Current development progress

Last updated: 2026-08-03

## Repository state

- Branch: `feature/command-timeline-v1.3.0`
- Starting commit: `395f9becd` (`main` after Command Timeline Phase 2)
- Microsoft Terminal upstream revision:
  `1cea42d433253d95c4487a3037db48197b5e72f4`
- Engineering application and PowerShell module version: `1.2.3`
- Engineering package version: `1.2.3.0`
- Intended checkpoint tag: `v1.2.3`
- Final Command Timeline release target: `v1.3.0`
- Current public Latest: `v1.2.0`
- Supported target: Windows 11 x64

`v1.2.3` is a development checkpoint, not a distributable release. The README
and GitHub Latest continue to identify v1.2.0 as the public Visual Progress
release. Checkpoint tags v1.2.1 through v1.2.4 run quick validation only and are
explicitly excluded from full build, installer packaging, asset publication,
and GitHub Release jobs.

## Implemented in the working tree

- Retained the Phase 1 pane-owned index and the Phase 2 navigation model,
  overlay, wheel accumulation, and accessibility surface unchanged as the only
  history data source and presentation path.
- Added a pane-owned pure C++ `CommandTimelineActionModel`. It decides whether
  a load, copy, or jump is possible from the stable selected `CommandId`, and
  never resolves output, reads the clipboard, or produces a payload containing
  a carriage return.
- Added Enter and single-click load onto the focused pane's input line. The
  payload is filtered for control codes only, `CarriageReturnNewline` is
  deliberately not applied, no carriage return is appended, and `SendInput`
  targets this pane's connection, so the load can never execute, never reads
  the Windows clipboard, and is never forwarded by input broadcast.
- Added multi-line and large-load protection: a multi-line command is refused
  when the shell has not enabled bracketed paste, and a load above 1024
  characters requires a confirming Enter. Escape cancels a pending confirmation
  before it closes the overlay.
- Added Space to jump the viewport to the selected command's native mark, and a
  per-entry context menu with copy command, copy output, and jump to output.
  Ctrl+C copies the selected command while the Timeline owns the keyboard.
- Added on-demand output resolution through
  `Terminal::ResolveCommandTimelineOutput`. Output is read from the buffer only
  for an explicit copy action and is never cached, indexed, or retained.
- Added `loadedCommandId` plus execution-generation tracking. `CommandStart`
  retires the loaded state, `IsCurrentGeneration` detects a late completion from
  a retired command, and `ReconcileLoadedInput` releases loaded-input state when
  the loaded command is evicted.
- Advanced authoritative engineering version surfaces to `1.2.3`/`1.2.3.0` and
  added the v1.2.3 root changelog entry.
- Kept Phase 4 out of scope: there is no search box, no filtering, and no
  public `commandTimeline.*` settings yet.

## Validation state

Phase 3 validation requires the focused Command Timeline model/control tests,
the extended `test-command-timeline.ps1` source and privacy boundaries, version
and branding verification, release/CI classification guards, shell integration
checks, repository Smoke validation, the smallest affected native projects, and
GitHub quick PR validation. Record exact results in the Draft PR and final task
report; do not treat this document as evidence for a command that did not run.

The annotated `v1.2.3` checkpoint tag must point to the final commit that passes
those gates. Its tag workflow must run checkpoint quick validation only and
must not create a GitHub Release or update Latest.
