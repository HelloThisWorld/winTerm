# Pane Handle Dragging

The detailed architecture, ownership boundary, supported transfer matrix,
rollback contract, and accessibility strategy are recorded in
[Pane Handle Snap Docking](pane-handle-snap-docking.md).

Pane docking starts only from the explicit drag grip in a pane header. Pointer movement in terminal content, the scrollbar, or an overflow button cannot start a pane drag. This preserves selection, mouse reporting, Vim/tmux interaction, application mouse input, and right-click copy/paste behavior.

## Drag lifecycle

The pane-handle source reuses the v0.5 docking components:

1. `DragThreshold` validates mouse or touch movement.
2. `DragPayloadRegistry` creates an opaque, expiring, single-use token.
3. `DockDragStateMachine` tracks press, threshold, drag, target, drop, commit,
   failure, rollback, completion, and cancellation.
4. `LayoutTransformer.BuildProposedLayout()` produces the proposed layout.
5. `DockPreview` derives preview geometry from that proposed tree.
6. `LayoutTransactionCoordinator` reserves live session ownership and commits or rolls back.

Hovering does not mutate the runtime layout, launch a shell, transfer a session, save a Workspace, write a file, or change focus.

## Zones

Top, Bottom, Left, and Right are always the core zones. Corner zones appear only when enabled and when the target is large enough. Center means move as a new tab; it never silently replaces or closes an existing pane. An Empty Slot is filled without adding a split.

Same-tab movement removes the source pane, normalizes the tree, and reinserts the same pane node around the target. It does not duplicate the source pane.

Dropping on the tab strip creates a standalone tab. Dropping outside the window requests a new same-process window. Cross-process live pane transfer is disabled with an explanation because the terminal session must not be restarted to imitate a move.

## Cancellation and failure

Escape, pointer-capture loss, invalid targets, closed source/target objects, display changes, expired tokens, and commit failure cancel or roll back the operation. The source remains in its original layout unless the complete target transaction succeeds.

Mixed-DPI adapters must express pointer and target rectangles in the target window's coordinate space before building the overlay or preview.
