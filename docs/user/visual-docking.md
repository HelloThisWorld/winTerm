# Visual Docking

Start pane docking from the grip in the pane header. Dragging terminal text, the scrollbar, or the overflow button does not move a pane.

The Snap-like overlay offers Top, Bottom, Left, and Right. Corner zones appear when enabled and when the target is large enough. Center over tab content or the tab strip means **Move to New Tab**; it never replaces a live pane. Empty Slots can be filled directly.

The preview comes from the same proposed layout used by commit. Escape or pointer-capture loss cancels without changing the layout. A failed commit rolls back the layout and session ownership.

Keyboard move mode uses Arrow keys for zones, Tab/Shift+Tab for targets, Enter to move, and Escape to cancel.

Live same-process pane transfer must preserve the Shell PID, ConPTY session, buffer, running command, alternate screen, and working-directory metadata. Cross-process live transfer is unsupported and is shown as disabled. Runtime Visual Docking remains gated by the [v0.7 acceptance report](../v0.7-acceptance.md).
