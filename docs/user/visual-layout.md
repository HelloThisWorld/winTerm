# Visual layout

winTerm uses a binary split tree. Every divider belongs to the immediate pair
of panes or nested pane groups on its two sides.

## Split

Use Top, Bottom, Left, or Right relative to the focused pane. A Top or Left
split inserts the new pane before the focused pane; Bottom or Right inserts it
after. Split operations preserve profile selection and roll back if shell
creation fails.

## Resize

Drag a vertical border to change left/right proportions or a horizontal border
to change top/bottom proportions. The visible line is thin, but the pointer
target is wider. The divider becomes brighter on hover and mint while active.

Default snap points are 25%, one third, 50%, two thirds, and 75%. Hold Alt to
bypass snapping. Escape and pointer cancellation restore the original ratio.
Only the split owning the divider changes; ancestor and unrelated nested ratios
remain unchanged.

Use **Balance Panes** for a valid 50/50 immediate split. Use **Undo Pane
Resize** and **Redo Pane Resize** to restore exact committed ratios.

## Restore

The final orientation, ratio, nested structure, focused pane, active tab, and
window geometry are included in normal persisted-layout restore. Hover,
pointer, snap label, modifier, and in-progress transaction state are not saved.
