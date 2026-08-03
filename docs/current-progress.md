# Current development progress

Last updated: 2026-08-04

## Repository state

- Branch: `feature/command-timeline-v1.3.0-phase4`
- Base branch: `feature/command-timeline-v1.3.0` (Command Timeline Phase 3,
  tag `v1.2.3`, pull request #29 awaiting owner merge)
- Microsoft Terminal upstream revision:
  `1cea42d433253d95c4487a3037db48197b5e72f4`
- Engineering application and PowerShell module version: `1.2.4`
- Engineering package version: `1.2.4.0`
- Intended checkpoint tag: `v1.2.4`
- Final Command Timeline release target: `v1.3.0`
- Current public Latest: `v1.2.0`
- Supported target: Windows 11 x64

`v1.2.4` is a development checkpoint, not a distributable release. The README
and GitHub Latest continue to identify v1.2.0 as the public Visual Progress
release. Checkpoint tags v1.2.1 through v1.2.4 run quick validation only and are
explicitly excluded from full build, installer packaging, asset publication,
and GitHub Release jobs.

## Implemented in the working tree

- Retained the Phase 1 index, the Phase 2 overlay and navigation model, and the
  Phase 3 load/copy/jump entry actions unchanged in behavior.
- Added pane-local search over each pane's bounded in-memory command text. The
  match is a literal case-insensitive substring search; there is no regex, no
  fuzzy matching, no output search, and no terminal-buffer rescan.
- Added a 256 UTF-16 code-unit query cap that truncates without leaving a lone
  surrogate, enforced in the model and mirrored by `MaxLength` on the filter box.
- Reworked the navigation model to walk a filtered projection while keeping
  stable `CommandId` identity. A still-matching command stays selected, a command
  that stops matching hands selection to the nearest surviving match, and a new
  command only takes the selection when it matches and the view was already
  following the latest command.
- Added `/` and Tab to focus the filter box. Both are consumed before the PTY,
  and filter-box text never reaches the shell. Escape now clears a non-empty
  query before it closes the overlay.
- Added the `commandTimeline.enabled` and `commandTimeline.historyLimit` global
  settings with defaults `true` and `500`, a 50–5000 clamped range, JSON schema
  entries, and a Settings UI section under Appearance. An absent setting is not
  serialized back, so existing settings files need no migration.
- Added bounded per-pane history with oldest-first eviction that applies to
  panes that already exist and to new panes. Raising the limit never resurrects
  an evicted command and sequence IDs are never reused.
- Added four distinct empty states so an unsupported shell is never reported as
  simply having run no commands.
- Made list item position and set size reflect the filtered result count, and
  kept localized accessible names, non-color status, High Contrast theme
  resources, and the Reduced Motion-safe no-animation path.
- Advanced authoritative engineering version surfaces to `1.2.4`/`1.2.4.0` and
  added the v1.2.4 root changelog entry.
- Kept `v1.3.0-alpha` out of scope: no persistent history, no output cache, no
  telemetry, and no public release work.

## Validation state

Phase 4 validation requires the focused Command Timeline model/control tests,
the Settings Model Command Timeline tests, the extended
`test-command-timeline.ps1` source, search, settings, and privacy boundaries,
version and branding verification, release/CI classification guards, repository
Smoke validation, the smallest affected native projects, and GitHub quick PR
validation. Record exact results in the Draft PR and final task report; do not
treat this document as evidence for a command that did not run.

The annotated `v1.2.4` checkpoint tag must point to the final commit that passes
those gates. Its tag workflow must run checkpoint quick validation only and
must not create a GitHub Release or update Latest.
