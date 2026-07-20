# Directed Split

winTerm 0.7 replaces an implicit universal split direction with four explicit choices:

| Command | Result relative to the focused pane |
|---|---|
| Split Pane Top | The new pane is placed above the focused pane. |
| Split Pane Bottom | The new pane is placed below the focused pane. |
| Split Pane Left | The new pane is placed to the left of the focused pane. |
| Split Pane Right | The new pane is placed to the right of the focused pane. |

The operation targets the currently focused pane node, including when that pane is nested. It never splits the complete tab root unless the root is itself the focused pane.

## Planning and commit

`DirectedSplitAction` validates the requested edge, focused-pane identity, profile selection, DPI, minimum dimensions, and final geometry before a shell is launched. It then asks the existing `LayoutTransformer` for the proposed tree:

- Top: horizontal split, new pane first.
- Bottom: horizontal split, focused pane first.
- Left: vertical split, new pane first.
- Right: vertical split, focused pane first.

The default ratio is 0.5. A valid plan is committed through a layout transaction and recorded in layout history. A rejected plan leaves the current layout unchanged and does not create a shell session.

## Profiles

Every direction retains profile selection. The pane-control setting can use the current default profile, the current pane profile, or require an explicit selection. The Ask Every Time mode cannot produce a split plan until a profile is selected.

The existing Terminal action schema continues to represent the four directions with `SplitDirection::Up`, `Down`, `Left`, and `Right`. Configurable keybindings and Command Palette entries use the same action path.

## Focus

By default, focus moves to the new pane after a successful split. The Pane Controls setting can retain focus on the existing pane.
