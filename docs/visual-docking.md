# Visual Docking

winTerm visual docking uses one layout model, transformer, preview path, transaction coordinator, session-ownership registry, and history implementation for tabs and panes.

Pane Handle dragging in 0.7 adds an explicit pane drag source and a Snap-like 3×3 overlay presentation. The mandatory edge zones are Top, Bottom, Left, and Right. Optional corners use existing Empty Slot nodes. Center creates a new tab or fills an explicitly selected Empty Slot; it never replaces a live target pane.

Preview geometry is calculated from the exact proposed layout returned by `LayoutTransformer.BuildProposedLayout()`. The commit receives the same plan. A preview that fails geometry validation cannot request a drop.

Same-process session reattachment is the only supported live cross-window path. Cross-process pane transfer remains disabled. See [Pane Handle Dragging](pane-handle-dragging.md) and [the v0.5 architecture](visual-docking-architecture.md).
