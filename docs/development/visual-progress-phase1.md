# Visual Progress Phase 1

Phase 1 provides the static, fail-open foundation for Visual Progress. It is a developer preview, not the finished v1.2.0 rainbow effect.

Phase 2 builds directly on this foundation; see [Visual Progress Phase 2](visual-progress-phase2.md) for the Rainbow Arc Weld renderer, bounded CLI recognition, and safe transient replacement preview.

## Architecture

The implementation reuses the standard terminal progress path:

`OSC 9;4` → terminal core taskbar state → `ControlCore` → `TermControl` → `TerminalPaneContent` → `Pane`

`src/winterm/VisualProgress/VisualProgressModel.h` normalizes each pane independently into a small snapshot containing only mode, status, value, visibility, source, and sequence. It never stores terminal output, command text, paths, environment contents, or other user data.

The existing OSC 133 semantic-mark parser remains authoritative for command lifecycle. A small event boundary now carries prompt, command-start, command-executed, command-finished, and exit-code state through the control layer. Visual Progress does not parse visible output or scan scrollback.

Explicit taskbar progress takes precedence while active. The mappings are:

| Existing state | Normalized mode | Normalized status |
| --- | --- | --- |
| Clear | Hidden, or current shell lifecycle fallback | Cancelled/hidden |
| Set | Determinate | Running |
| Indeterminate | Indeterminate | Running |
| Error | Determinate | Error |
| Paused | Determinate | Waiting |
| OSC 133 command start/executed | Indeterminate | Running |
| OSC 133 command finish, exit 0 | Determinate at 100 | Success |
| OSC 133 command finish, nonzero exit | Determinate at 100 | Error |
| Next OSC 133 prompt | Hidden | Cancelled/hidden |

Determinate values are clamped to 0–100. Duplicate snapshots are suppressed. Event ingress uses try-lock-and-drop semantics, and a one-element mailbox coalesces rapid updates before low-priority UI dispatch, so terminal output never waits for the decorative UI, pending work is bounded, and the newest accepted visual state wins.

## Preview feature gate

The setting defaults to off. Developers can enable it as a global setting in `settings.json`:

```json
{
  "visualProgress.enabled": true
}
```

Set `WINTERM_DISABLE_VISUAL_PROGRESS=1` before starting winTerm to disable the feature regardless of the setting. The emergency override always wins. When disabled, panes do not create the overlay, schedule UI updates, start timers, or create worker threads.

## Static overlay and fail-open behavior

Each enabled leaf pane owns one six-logical-pixel overlay in the existing content row. It is layered over the terminal, uses ten-pixel horizontal margins, never changes terminal rows or columns, is excluded from accessibility and hit testing, and is rebuilt or removed with the pane visual lifecycle. XAML logical sizing covers 100%, 125%, 150%, and 200% DPI, including narrow and zoomed panes.

The track and status fills use dark, light, and High Contrast theme resources. Determinate width matches the normalized value; indeterminate progress uses a fixed centered segment and never presents a fake percentage.

All overlay creation, dispatch, and update failures are caught and logged through the existing application logging macros. A failed pane overlay disables itself without affecting the terminal process, PTY, input, output, selection, copy/paste, or pane controls. No progress state is persisted to workspaces and no progress telemetry is emitted.

## CI classification

The PR workflow computes the exact base-to-head diff and chooses one class:

- `docs-only`: conservative documentation allowlist; quick validation only.
- `code`: quick validation plus one x64 Release build and relevant compiled tests.
- `delivery`: workflow, version, installer, packaging, manifest, dependency, or build-system changes; quick validation, x64 Debug and Release builds, relevant compiled tests, unpackaged/Setup/Portable builds, lifecycle tests, and the exact artifact allowlist.

The `ci:full` or `delivery` label forces delivery validation. Manual dispatch accepts `auto`, `fast`, or `full`. The final `ci-gate` job always appears and rejects any missing, failed, or cancelled required job. The separate tag-triggered `release.yml` remains authoritative and unchanged.

Branch protection should require `ci-gate` after this workflow lands.

## Testing

Run source-only validation:

```powershell
.\scripts\winterm\test-visual-progress.ps1 -SourceOnly
```

After building compiled tests, run:

```powershell
.\scripts\winterm\test-visual-progress.ps1 -Configuration Release -Platform x64 -RequireCompiled
```

To exercise standard progress and semantic command states in an enabled winTerm pane without Docker, pip, or another CLI provider:

```powershell
.\scripts\winterm\invoke-visual-progress-smoke.ps1
```

## Phase 1 non-goals

Phase 1 deliberately excludes animation, rainbow gradients, glow, bloom, sparks, interpolation, moving indeterminate effects, CLI-specific parsers, output suppression, labels, ETA, speed, notifications, and Settings UI polish. It adds no output-line parser, scrollback scan, background polling, persistent timer, or worker thread.
