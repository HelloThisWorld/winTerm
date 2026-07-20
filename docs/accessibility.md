# Pane Control Accessibility

Pane controls expose a named pane header, a Button-role drag grip, focused state, visible status text, context-menu action, drag action, and keyboard alternatives.

Examples:

- `Move PowerShell pane`
- `Open pane menu`
- `Split pane above`
- `Split pane below`
- `Split pane to the left`
- `Split pane to the right`

Internal pane IDs, session pointers, and Layout node IDs are never included in announcements.

Keyboard move mode announces the source, selected target and zone, and commit/cancel instructions. For example: `Moving PowerShell pane. Target: top of Command Prompt pane. Press Enter to move. Press Escape to cancel.`

High Contrast presentation uses system colors and retains labels/borders. Focus, running, failed-command, and read-only states are not conveyed only by color.
