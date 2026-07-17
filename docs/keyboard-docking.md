# Keyboard docking

## Command surface

`KeyboardDockingController::Commands` defines stable command IDs for moving a tab to a new, previous, or next window; docking a tab to four edges or four corners; moving a pane to a new tab or window; docking a pane to four edges; and undoing or redoing a layout change.

The command descriptors do not assign default shortcuts. The Settings and command-palette adapters must use the existing configurable keybinding system so v0.5 does not overwrite upstream shortcuts.

## Docking mode

The keyboard model has four externally visible states:

- `Inactive`
- `SelectingZone`
- `CommitRequested`
- `Cancelled`

Starting the mode requires a source label, target label, and at least one available zone. Arrow keys move through the available cells of the same 3-by-3 zone model used by the overlay. Disabled or hidden zones are skipped. Enter requests a commit; it does not mutate a layout itself. Escape cancels and leaves the original layout unchanged.

The runtime adapter must pass an Enter request through the same `DockingPlan`, validation, transaction, ownership lease, and rollback path used by pointer docking.

## Announcements

Announcements include the moving source, selected side or corner, target window, and Enter/Escape instructions. For example:

```text
Moving PowerShell tab. Target: right side of Titan Mind window. Press Enter to dock. Press Escape to cancel.
```

After Escape:

```text
Docking cancelled. The original layout is unchanged.
```

Labels must be localized when the runtime UI is connected. No terminal command, output, clipboard content, or full working directory belongs in an announcement.

## Runtime acceptance

The source model and compiled unit-test cases are present, but command-palette registration, keybinding configuration UI, Narrator output, focus restoration, and end-to-end commit remain unverified. Keyboard docking must remain behind the runtime readiness gate until those checks pass.
