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

## Search

| Shortcut | Behavior |
| --- | --- |
| `Ctrl+F` | Open search in the focused pane |
| `Ctrl+Shift+F` | Open search in the focused pane (compatibility alias) |

Search always targets the active pane. The search box opens as an overlay
inside that pane without resizing terminal content, and the input field is
focused immediately. Typing searches the pane's buffer and scrollback as you
type and highlights every match at once, with a `current / total` counter
beside the input. While search is open, the scrollbar shows an overview:
one marker per buffer row containing a match, with the current match's row
drawn wider. The overview appears even when the generic
`showMarksOnScrollbar` setting is off, and it disappears when search
closes; if the scrollbar itself is hidden by configuration, search simply
runs without the overview. `Enter` moves to the next match and
`Shift+Enter` to the previous one, wrapping around at either end. `Esc` or
the close button dismisses the search box, clears the highlights, and
returns focus to the terminal. Text typed into the search box never reaches
the shell. In narrow panes the box compacts itself, keeping the input and
close button usable.

Both chords are ordinary configurable keybindings. A terminal application
that needs a literal `Ctrl+F` keystroke can reclaim it by unbinding the
default (`{ "command": "unbound", "keys": "ctrl+f" }` in settings); Find
stays reachable through `Ctrl+Shift+F` or the Command Palette.

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
