# Docking zones

Visual Docking resolves targets in this order:

1. tab-strip insertion;
2. empty layout slot;
3. pane-level target;
4. window-content target;
5. new-window target.

Only one target family is active at a time. Leaving a window clears its overlay. A target change requires the configured dwell or pointer distance so adjacent zones do not flicker.

## Zone behavior

| Target | Zones | Result |
| --- | --- | --- |
| Tab strip | insertion positions | Reorder or move as a tab |
| Window content | center | Move as a new tab |
| Window content | edges | Split around the active tab root |
| Window content | corners | Nested split with one empty slot |
| Pane | edges | Split around the identified pane |
| Pane | corners | Nested split when geometry permits |
| Empty slot | slot body | Replace the slot |
| Outside all windows | new window | Move after the tear-out threshold |

The pane center never silently replaces or closes a running target.

## Capability filtering

A zone is hidden or disabled when the target is closing, layout changes are restricted, the pane limit would be exceeded, geometry is too small, the zone is disabled in Settings, the session type cannot be retained, or the destination belongs to another process. A disabled zone exposes its reason to pointer, keyboard, and assistive-technology users.

Cross-process destinations use the message:

`Live terminal transfer is available only between windows in this winTerm process.`

## Preview

`LayoutTransformer` produces the proposed tree and `LayoutGeometry` derives rectangles from that tree. The same proposed tree is passed to validation and commit. Hovering never starts a shell, transfers ownership, changes focus, marks Workspace state dirty, writes a file, or records history.
