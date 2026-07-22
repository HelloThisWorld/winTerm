# Pane Handle Snap Docking

## Current architecture

The pane header is constructed by `TerminalApp::Pane::_AttachLeafVisual`. It owns
a compact grip before the pane title, a status label, and the existing overflow
button. The header occupies its own fixed-height row above the `TermControl`, so
header input is not forwarded to the terminal and does not overlap the terminal
content or scrollbar.

The reusable docking implementation is model-first and is compiled into the
Settings Model library. There is no separate class named `DockDragController` in
this revision. Its responsibilities are deliberately split between
`PaneHandleDragSource`, `DockDragStateMachine`, `DockTargetResolver`, and
`LayoutTransactionCoordinator`; this is the existing controller path and must be
used by a runtime adapter rather than replaced by another docking engine.

`WindowEmperor` owns the normal application windows in one winTerm process.
Each window has its own UI thread and `TerminalPage`, while a process-wide
`ContentManager` retains attachable terminal content. A `TermControl` is the
thread-affine view; its retained `ControlInteractivity`/`ControlCore` owns the
buffer, renderer state, and `ITerminalConnection`. Elevated windows and another
launched winTerm instance use another process.

The native pane header renders and focuses correctly, and its handle and
overflow button share the terminal's unified context flyout. Its runtime adapter
captures pointer input only from the grip, feeds the existing
`PaneHandleDragSource` and `DockTargetResolver`, renders the model-produced Snap
zones and proposed-layout preview, and commits same-tab edge drops through the
layout transaction coordinator. Center drop moves the same live pane into a new
tab. Cross-window and outside-window handle drops remain readiness-gated. Model,
source, and compiled tests must not be reported as proof that the live XAML drag
interaction passed.

## Reused v0.5 components

- `PaneHandleDragSource` accepts only `PanePointerRegion::DragGrip`, applies the
  DPI-scaled system threshold supplied by the host, owns the expiring drag token,
  and exposes the explicit drag lifecycle.
- `DockDragStateMachine` covers `Idle`, `PointerPressed`, `DragPending`,
  `Dragging`, `TargetAcquired`, `DropPending`, `Committing`, `Completed`,
  `Cancelled`, `Failed`, and `RollingBack`.
- `DockTargetResolver` applies tab-strip, Empty Slot, pane, window-content, and
  new-window priority with target hysteresis.
- `DockingOverlayModel` and `PaneSnapLayoutOverlay` describe the 3×3 Snap-like
  layout, disabled reasons, target-DPI bounds, accessible names, and selected
  state without taking focus.
- `LayoutTransformer::BuildProposedLayout` is the single source for preview and
  commit trees. `DockPreview` calculates geometry from that exact proposal.
- `SessionOwnershipRegistry` and `SessionTransferLease` enforce one live owner
  per session while a view is reattached.
- `LayoutTransactionCoordinator` sequences validation, target preparation,
  ownership reservation, model and visual commit, Workspace dirty tracking,
  history, focus restoration, and cleanup.
- `LayoutHistory` retains in-memory undo/redo entries only while every referenced
  live session remains available.
- Workspace schema 2 persists pane, split, and Empty Slot layouts and stable IDs;
  drag tokens, pointer state, overlay state, and leases are transient.

## Same-window transfer behavior

The supported model operations move the existing pane node, never a recreated
shell. Edge drops remove and normalize the source tree and insert that same pane
above, below, left, or right of the target with a default 0.5 ratio. Corner drops
reuse the deterministic split-plus-Empty-Slot representation. Center on an Empty
Slot fills that slot. Center on the tab strip or window content proposes a
standalone tab containing the source pane.

Within one process, the existing content manager can retain the live terminal
core while a destination view is attached. The shell process, ConPTY connection,
buffer, scrollback, running command, CWD and exit-code metadata, shell integration
metadata, and alternate buffer therefore have a technically valid preservation
path. Stable-release evidence still requires live runtime tests that compare
these identities before and after every supported move.

## Cross-window transfer behavior

Normal winTerm windows created by the same `WindowEmperor` are in the same
process. The inherited tab tear-out and content-ID path demonstrates same-process
content reattachment through `ContentManager`, but pane-handle cross-window
commit must remain disabled until the pane-specific two-sided transaction and
mixed-DPI UI adapter pass runtime rollback tests.

Cross-process live pane transfer is unsupported. This includes an elevated
window, a separately launched instance owned by another process, and a target
whose source application-instance identity differs. Such targets must be
disabled with:

> Live terminal transfer is available only between windows in this winTerm process.

There is no shell-restart, command-replay, working-directory-only, or serialized
buffer fallback. `Move to New Window` must remain disabled when the destination
cannot be proven to share the live content registry.

## Unsupported transfer scenarios

- Cross-process, cross-elevation, or different application-instance transfers.
- Exited or non-transferable connections.
- A source or target pane, tab, or window that is closing or changes ownership
  generation during prepare/commit.
- Layouts that exceed minimum geometry, ratio, depth, or pane-count limits.
- New-window and cross-window handle drops until the runtime adapter and
  transaction callbacks have passed the required live-session tests.
- Undo or redo after a required live session or destination window is gone.

Unsupported targets are disabled before commit and leave the source untouched.

## Rollback strategy

Every enabled drop captures both layouts, focus, zoom, and generation-tagged
owners before acquiring a `SessionTransferLease`. Target preparation cannot
delete source content. Ownership is reserved before publishing the model; the
visual tree is attached before the source is finally removed. Workspace dirty
tracking and history occur only after exactly-one-owner validation succeeds.

Any failure invalidates the drag token, removes the overlay, restores ownership,
restores both snapshots and focus, closes temporary hosts, and releases the
lease. The user-visible result is:

> The pane could not be moved. The previous layout was restored.

If ordinary rollback cannot restore the original tree, `recoverSessions` must
place every retained live session into a safe standalone tab. Autosave must not
observe or persist a failed intermediate layout.

## Accessibility strategy

The visible grip stays compact while the header button supplies a padded,
DPI-aware hit target. A normal click focuses the pane; only movement past the
host-provided system threshold can begin dragging. The handle name is
`Move <title> pane`, and the shared overflow action is `Open pane menu`.

Overlay zones expose names including `Dock pane above target`, `Dock pane below
target`, `Dock pane to the left of target`, `Dock pane to the right of target`,
and `Move pane to a new tab`. Disabled reasons are appended to the automation
name and shown by the host as help text or a tooltip. Internal IDs, pointers, and
tokens are never announced.

Focus is communicated by the pane border and the textual `Active` status, not by
color alone. High Contrast uses system control semantics, and the overlay model
does not take terminal keyboard focus. Keyboard move mode retains Tab/Shift+Tab
target cycling, arrow-zone selection, Enter to commit, and Escape to cancel.
