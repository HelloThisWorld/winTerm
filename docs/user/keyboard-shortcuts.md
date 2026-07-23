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
