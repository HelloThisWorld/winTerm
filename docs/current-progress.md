# Current development progress

Last updated: 2026-08-02

## Repository state

- Branch: `feature/command-timeline-v1.3.0`
- Starting commit: `abb581a2a` (`main` after Command Timeline Phase 1 and the
  label-gated CI/test-hang follow-up)
- Microsoft Terminal upstream revision:
  `1cea42d433253d95c4487a3037db48197b5e72f4`
- Engineering application and PowerShell module version: `1.2.2`
- Engineering package version: `1.2.2.0`
- Intended checkpoint tag: `v1.2.2`
- Final Command Timeline release target: `v1.3.0`
- Current public Latest: `v1.2.0`
- Supported target: Windows 11 x64

`v1.2.2` is a development checkpoint, not a distributable release. The README
and GitHub Latest continue to identify v1.2.0 as the public Visual Progress
release. Checkpoint tags v1.2.1 through v1.2.4 run quick validation only and are
explicitly excluded from full build, installer packaging, asset publication,
and GitHub Release jobs.

## Implemented in the working tree

- Retained the Command Timeline Phase 1 pane-owned index, bounded command-text
  cache, stable IDs, native mark identity, incremental OSC 133 lifecycle, warm
  bootstrap, and clear/eviction/reflow cleanup as the only history data source.
- Added a pane-owned pure C++ navigation and presentation model. It restores a
  valid selected ID, native anchor, and visual slot; defaults to the latest
  command; chooses the nearest surviving ID after removal; and materializes
  only the visible rows required by the pane.
- Added a TermControl left-side handle and overlay layered over the terminal
  surface. It does not change terminal layout, padding, swap-chain or PTY size,
  and its closed state leaves all other terminal pointer input untouched.
- Added Up/Down one-entry movement, page-edge one-in/one-out viewport movement,
  Left/Right current-page edge selection, hover/click-only selection, Escape
  close, and focused-pane toggle routing through the existing action system.
- Added pane-local wheel/trackpad delta accumulation and a lifetime-safe settle
  timer. Partial deltas are accumulated, direction reversal cancels unfinished
  motion, full thresholds move complete rows, and hide/close stop the timer and
  clear UI-only entries.
- Added localized list/list-item accessibility, open/close handle names,
  command-plus-status names, non-color status glyphs, High Contrast-aware theme
  resources, and a Reduced Motion-safe path without a continuous animation.
- Remapped canonical defaults to `Ctrl+Tab` (Timeline), `Ctrl+T` (next tab),
  `Ctrl+Shift+T` (previous tab), and `Ctrl+Alt+T` (new tab), while retaining
  user-defined keybinding precedence and existing IME/AltGr ordering.
- Advanced authoritative engineering version surfaces to `1.2.2`/`1.2.2.0`,
  fixed the README Windows CI badge, added v1.2.1 and v1.2.2 root changelog
  entries, and established the permanent root changelog/Wiki ledger policy.
- Kept Phase 3 out of scope: entry click and hover only select. They do not
  insert, paste, copy, execute, close, or jump to command output.

## Validation state

Phase 2 validation requires the focused Command Timeline model/control tests,
Settings Model shortcut tests, TerminalApp action routing checks, XML/XAML/JSON
and PowerShell parsing, version/checkpoint guards, repository Smoke validation,
the smallest affected native projects, README badge verification, and GitHub
quick PR validation. Record exact results in the Draft PR and final task report;
do not treat this document as evidence for a command that did not run.

The annotated `v1.2.2` checkpoint tag must point to the final commit that passes
those gates. Its tag workflow must run checkpoint quick validation only and
must not create a GitHub Release or update Latest.
