# Keyboard shortcuts

winTerm uses the configurable Microsoft Terminal keybinding system. Review or
assign bindings in Settings or the Command Palette.

Pane layout commands include:

- Split Pane Top, Bottom, Left, and Right
- Resize Pane Left, Right, Up, and Down
- Balance Panes
- Undo Pane Resize
- Redo Pane Resize
- Close Focused Pane
- Open Pane Menu

Keyboard resize changes the immediate split containing the focused pane by 2%.
Hold **Shift** for a 5% step. Valid configured snap points are evaluated while
crossing them. Keyboard layout changes create one history entry and preserve
the running shell.

During pointer resizing, hold **Alt** for free resize or press **Escape** to
cancel. Those keys are handled by the divider and are not sent to the shell
while it owns pointer capture.

Pane movement commands are not available in winTerm 1.1. A legacy custom
`movePane` action can still be parsed safely but is disabled.

## Command Timeline

| Shortcut | Behavior |
| --- | --- |
| `Ctrl+Tab` | Toggle the Command Timeline for the focused pane |
| `Ctrl+T` | Next tab |
| `Ctrl+Shift+T` | Previous tab |
| `Ctrl+Alt+T` | Open new tab |

While the Command Timeline is open:

| Key | Behavior |
| --- | --- |
| `/` or `Tab` | Move focus to the filter box |
| `Up` / `Down` | Move the selection by one command |
| `Left` / `Right` | Select the first / last command on the current page |
| `Enter` | Load the selected command onto the input line; never runs it |
| `Space` | Scroll the terminal to that command's output |
| `Ctrl+C` | Copy the selected command text |
| `Escape` | Clear the filter, or close the Timeline if the filter is empty |

Explicit user key bindings take precedence over all of these defaults. Keys the
Timeline consumes are not sent to the shell, and text typed into the filter box
never reaches the shell. See [Command Timeline](command-timeline.md) for the
full behavior.
