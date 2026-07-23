# Keyboard pane commands

winTerm exposes configurable actions through the inherited keybinding and
Command Palette system:

| Action | Behavior |
| --- | --- |
| `splitPane` with `splitDirection: up` | Split Pane Top |
| `splitPane` with `splitDirection: down` | Split Pane Bottom |
| `splitPane` with `splitDirection: left` | Split Pane Left |
| `splitPane` with `splitDirection: right` | Split Pane Right |
| `resizePane` with a direction | Resize by 2%; hold Shift for 5% |
| `balancePanes` | Balance the immediate sibling split |
| `undoPaneResize` | Restore the previous committed pane ratio |
| `redoPaneResize` | Reapply the undone pane ratio |
| `closePane` | Close the focused pane |

Default command IDs are `Terminal.BalancePanes`,
`Terminal.UndoPaneResize`, and `Terminal.RedoPaneResize`. Users may assign
keys without overwriting existing bindings.

The legacy `movePane` identifier is accepted during settings parsing for
compatibility but is disabled in the winTerm application and has no default,
menu, command-line, or runtime move implementation.
