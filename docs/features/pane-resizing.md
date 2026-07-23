# Pane border resizing

Pane layout is changed by dragging the border between sibling panes. Pane
headers are informational controls and never initiate repositioning.

The visible divider is one logical pixel. A centered 12-logical-pixel pointer
target makes it practical to acquire without changing the layout. Vertical
dividers use the east-west resize cursor and change left/right proportions;
horizontal dividers use the north-south cursor and change top/bottom
proportions.

## Transaction

1. Pointer press records the owning split ID, original ratio, local container
   size, and both immediate child minimum sizes.
2. Pointer movement clamps the candidate ratio, applies optional snapping, and
   updates the existing branch row/column definitions continuously.
3. Pointer release commits one normalized ratio and one history record.
4. Escape, pointer cancellation, or pointer-capture loss restores the original
   ratio and creates no history record.

No workspace serialization or disk write occurs on pointer movement. Normal
persisted-layout capture serializes only the final split ratio.

## Keyboard and history

The inherited Resize Pane Left/Right/Up/Down commands use a 2% ratio step;
holding Shift uses 5%. Keyboard resizing evaluates the same valid snap points.
**Balance Panes** sets the immediate split containing the focused pane to
50/50 when both children satisfy minimum sizes. **Undo Pane Resize** and
**Redo Pane Resize** restore exact committed ratios.

## Nested layouts

A divider belongs to one binary split node. Its calculations use that node's
width or height, never the complete window, so resizing a nested horizontal
split preserves its ancestor vertical ratio and vice versa.
