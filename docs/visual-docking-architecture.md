# winTerm visual docking architecture

## Status

The v0.5 docking core is model-first and is compiled as part of the existing Settings Model library. The runtime XAML adapter remains disabled until the Windows build, transaction rollback tests, and multi-window manual checks can be run. This prevents a partially verified drop path from removing live terminal content.

## Request flow

1. A tab, pane, or complete tab layout becomes a `DockSource`.
2. Target adapters publish `DockTargetCandidate` instances in screen-relative logical coordinates.
3. `DockTargetResolver` applies the documented priority order and hysteresis.
4. `DockingOverlayModel` derives enabled zones from `DockingCapabilities`.
5. `LayoutTransformer::BuildProposedLayout` creates an immutable `DockingPlan`.
6. `DockPreview` derives geometry from that proposed layout.
7. `LayoutTransactionCoordinator` captures state, acquires temporary session leases, prepares the target, commits the model and visual tree, transfers ownership, marks the Workspace dirty, and records history.
8. Any failure invokes rollback and restores the captured ownership and layouts.

The preview and committed layout therefore share the same tree transformation and geometry algorithms. Pointer hover does not mutate the layout, transfer ownership, or write a Workspace.

## Sources, targets, and zones

Sources are complete tabs, individual panes, or an internal layout subtree. A complete multi-pane tab retains its root split tree. Targets are tab strips, window content, panes, empty slots, or a new window.

Target priority is:

1. Tab strip insertion
2. Empty slot
3. Pane
4. Window content
5. New window

The window-content overlay supports center, four edges, and four corners. Center means move as a new tab. Pane adapters may suppress corner targets when the target cannot meet minimum pane geometry. Empty slots replace the slot node without creating a split.

## Session and process boundary

The inherited `ControlCore` retains the terminal connection, terminal buffer, and renderer state while a `TermControl` view is detached. Normal windows are coordinated by `WindowEmperor` in the same process and share a `ContentManager`. A `SessionOwnershipRegistry` gives each live session exactly one pane owner and provides a temporary lease for transfer.

The v0.5 architecture permits live transfer only between windows in the same winTerm process. Cross-process transfer is rejected. There is no shell restart fallback, external broker, live-session duplication, or persistence after winTerm exits.

## Runtime adapters

The remaining UI adapter has these responsibilities:

- Convert XAML pointer coordinates to target-window logical coordinates.
- Publish tab-strip, pane, empty-slot, window-content, and new-window candidates.
- Render the overlay without taking keyboard focus.
- Render preview regions and text alternatives.
- Prepare the destination view before releasing the source.
- Bind command-palette actions and user keybindings to `KeyboardDockingController`.
- Feed the committed model to Workspace dirty tracking and persistence.

`DockingSettingsModel::enableRuntimeDocking` defaults to `false`. The adapter may enable it only when same-process hosting, view reattachment, compiled rollback tests, and runtime UI verification are all true.

## Workspace integration

Workspace schema 2 serializes pane, split, and empty-slot nodes, stable slot IDs, a docking model version, and optional layout-history metadata. Runtime drag state, overlay state, tokens, leases, and terminal output are never serialized. Version 1 migration is deterministic and creates a backup before persistent migration.

## Protected upstream components

Visual docking does not modify ConPTY, the VT parser, text buffer, renderer core, Unicode width engine, input protocol parser, or OpenConsole internals. Runtime integration should use existing tab tear-out, `ContentManager`, pane tree, and `TermControl` attachment seams.

## Current limitations

- The XAML overlay, pointer adapter, and command-palette registration are not connected.
- Live tab and pane movement has not been built or manually verified in this environment.
- Cross-process windows are intentionally unsupported.
- Mixed-DPI new-window placement and monitor-removal cancellation require runtime validation.
- Undo and redo are in-memory and valid only while referenced live sessions remain available.
