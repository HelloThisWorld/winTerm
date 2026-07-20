# Keyboard Pane Commands

winTerm exposes configurable command IDs without assigning unconditional default shortcuts:

| Command ID | Command |
|---|---|
| `splitPaneTop` | Split Pane Top |
| `splitPaneBottom` | Split Pane Bottom |
| `splitPaneLeft` | Split Pane Left |
| `splitPaneRight` | Split Pane Right |
| `movePaneToNewTab` | Move Pane to New Tab |
| `movePaneToNewWindow` | Move Pane to New Window |
| `closeFocusedPane` | Close Focused Pane |
| `startPaneMoveMode` | Start Pane Move Mode |
| `openPaneMenu` | Open Pane Menu |

Keyboard move mode uses Arrow keys to select a zone, Tab/Shift+Tab to change targets, Enter to request the move, and Escape to cancel. It remains available when pane headers are hidden.

The existing Terminal action schema is retained for serialized split and move actions. No existing user keybinding is overwritten.
