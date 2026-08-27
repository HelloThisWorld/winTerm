# Current development progress

Last updated: 2026-08-27

## Repository state

- Working branch: `fix/v1.4.3-click-position`
- Branch base: `0ee3adcb63c9cb028a44badeddb0362db38fa5f7`
  (`origin/main` at hotfix preparation start)
- Application version: `1.4.3`
- Package/file version: `1.4.3.0`
- PowerShell module version: `1.4.3` with an empty prerelease suffix
- Release channel: `stable`
- Intended tag: `v1.4.3`
- Supported target: Windows 11 x64

The source remains based on the repository's pinned Microsoft Terminal
baseline `release-1.25@1cea42d433253d95c4487a3037db48197b5e72f4`.
Microsoft Terminal `upstream/main` was separately refreshed through
`c7572cde0c69733e4511787dc963eb336f17adbf` (2026-08-25) for the required
upstream-state check. No broad upstream merge was performed.

## 1.4.3 hotfix scope

winTerm 1.4.3 is a focused production regression hotfix for Click to position
cursor. The feature shipped in the 1.4.1 payload and remained unchanged in
1.4.2. Real physical mouse testing showed that small movement between
mouse-down and mouse-up could intermittently reject the first click and could
leave later clicks unable to reposition the cursor.

No unrelated feature work is included. Pane Search, Command Timeline, Visual
Progress, workspace behavior, package identity, and protocol/schema versions
remain unchanged.

Visual Progress remains unchanged in 1.4.3; this hotfix does not alter its
rendering, recognition, accessibility, privacy, or per-pane behavior.

## Confirmed root cause

The press path correctly recorded a plain single left click as a pending cursor
reposition and saved its touchdown position. The move path calculated the
existing drag threshold correctly, but then called `SetEndSelectionPoint()`
even when movement was still below that threshold.

With no active selection, `ControlCore::SetEndSelectionPoint()` returned
without changing terminal selection. `ControlInteractivity` nevertheless set
`_selectionNeedsToBeCopied = true`, producing this invalid state:

```text
HasSelection()                 = false
_cursorRepositionPending      = true
_selectionNeedsToBeCopied     = true
```

The release path then required the copy flag to be false before repositioning.
Normal 1-pixel jitter could therefore reject an otherwise valid click, and the
stale copy flag could remain set into future gestures.

The bug was reproduced before production changes with a compiled deterministic
press, 1-pixel move, release test. It failed exactly on the phantom copy-state
assertion while confirming no selection existed and cursor reposition remained
pending.

## State-machine correction

- Pointer movement below the existing drag threshold performs no selection
  operation. Cursor reposition remains pending and selection-copy state is not
  modified.
- Crossing the threshold cancels cursor reposition, establishes the selection
  anchor once, updates the selection end, and transfers ownership to the drag.
- Moves after the threshold continue updating only the selection end; the
  anchor is not re-established.
- `ControlCore::SetEndSelectionPoint()` now reports whether an active selection
  was actually updated. Interactivity marks copy state dirty only on success.
- `PointerReleased()` uses `_cursorRepositionPending` and the established
  modifier/VT/Core safety checks to decide click ownership. It does not use the
  selection-copy flag as the primary click proof.
- A release with no active selection normalizes stale copy state without
  clearing or damaging legitimate mark-mode or CopyOnSelect state while a real
  selection exists.

Ctrl+Click hyperlinks, VT mouse reporting, double-click word selection,
triple-click line selection, Shift+Click, drag selection, and CopyOnSelect keep
their existing precedence. The 1.4.1/1.4.2 negative-coordinate, padding,
viewport, buffer-bound, historical-output, finished-mark, editable-range,
full-width-glyph, and split-pane safeguards are unchanged.

## Regression coverage

New deterministic Control tests cover:

- a 1-pixel move remaining a click with no phantom selection-copy state;
- several alternating sub-threshold moves remaining a click;
- ten independent cursor-position clicks at different editable positions;
- a jittered gesture followed by another successful ordinary click;
- exact below-threshold and at-threshold behavior;
- drag selection continuing to update after the threshold without cursor
  input;
- CopyOnSelect copying real drag selection but not sub-threshold jitter;
- recovery from `_selectionNeedsToBeCopied = true` while no selection exists.

The existing cursor tests continue to cover release-only positioning,
Shift+Click, double/triple click, explicit disablement, editable shell marks,
historical output, padding, wrapped commands, CJK/full-width glyphs, and
split-pane connection isolation.

Current local automated results:

- focused cursor-position family: PASS, 11/11;
- complete Debug x64 compiled Control suite: PASS, 100/100;
- complete Debug x64 Relevant suite: PASS (Settings Model 245/245,
  TerminalApp 51/51, Control 100/100);
- complete Release x64 application and compiled-test build: PASS;
- complete Release x64 Relevant suite: PASS (Settings Model 245/245,
  TerminalApp 51/51, Control 100/100);
- Smoke suite: PASS;
- version verification: PASS for application 1.4.3, package/file 1.4.3.0,
  PowerShell module 1.4.3, stable channel, and tag v1.4.3;
- branding verification with publisher `CN=helloThisWorld`: PASS;
- source-only Visual Progress verification: PASS;
- unpackaged Release x64 stage generation and layout verification: PASS.

## Real GUI validation gate

The real Release x64 application was launched in isolated portable mode with
PowerShell 7 and an unexecuted `git commit --amend --no-edit` command. An
OS-input validation run used actual absolute mouse-move packets between
button-down and button-up. It passed 30/30 cursor-position clicks across the
beginning, middle, and end of the line, alternating left and right, including
ten rapid far-apart clicks. Every gesture included 1-pixel X and Y movement,
inserted a temporary marker at the asserted command index, restored the
original command, and left no selected text. A paced drag selected
`commit --a`, a double click selected exactly `commit`, and a triple click
selected the complete line.

This OS-injected real-window evidence is supplemental and is not represented
as manual physical-mouse validation. The required physical-mouse GUI gate is
still **NOT RUN**. Five verified execute/new-prompt lifecycles are also not yet
recorded because the isolated window closed during that attempted sequence.
Release readiness therefore still requires manual mouse validation with normal
physical jitter, at least 30 cursor-position clicks, separate click timing
versus genuine double/triple click, five prompt lifecycles, and PowerShell 7 at
minimum. PowerShell 5, cmd.exe, WSL, and representative VT mouse applications
should also be exercised where available.

The 1.4.3 release must not be tagged or described as complete until this gate
has actual evidence.

## Release channel and artifacts

The guarded tag workflow must confirm an exact tag/version match, absence of
an existing Release, a clean checkout, version/branding/security/privacy gates,
an x64 Release build with compiled tests, artifact generation, Draft Release
round-trip validation, publication, and public asset re-download.

The expected public Release assets are:

- `winTerm-1.4.3-setup-x64.exe`
- `winTerm-1.4.3-portable-x64.zip`
- `SHA256SUMS.txt`
- `THIRD_PARTY_NOTICES.md`
- `SBOM.spdx.json`
- `SBOM.cyclonedx.json`
- `release-metadata.json`
- `winTerm-1.4.3-release-notes.md`

The installer is not currently Authenticode-signed, so Unknown Publisher or
SmartScreen warnings remain possible. The Release notes disclose this and
direct users to verify `SHA256SUMS.txt`.

## Release plan

1. Complete the final diff audit after all validation records are updated.
2. Complete and record the manual physical GUI mouse-jitter, repeated-click, selection,
   prompt-lifecycle, shell, and VT mouse validation gate.
3. Commit the focused 1.4.3 source/docs change, publish the matching Wiki
   ledger entry, push the branch, and merge through the application pull
   request only after it is genuinely release-ready.
4. Synchronize `main`, create annotated tag `v1.4.3` on the exact merged commit,
   push only that tag, and monitor the formal Release workflow to a terminal
   result.
5. Verify GitHub Latest and every public asset and checksum, then update
   `winterm-site` from the real Release URL, `published_at`, asset URLs, and
   filenames. Publish and verify English/Japanese production parity.
