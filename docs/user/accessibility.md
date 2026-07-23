# Accessibility

Pane icons are announced as **_title_ pane** and focus the pane when activated.
They do not expose movement language or a movement cursor. Overflow buttons are
announced as **Open pane menu**, and directed split commands describe Top,
Bottom, Left, or Right.

Dividers are keyboard-focusable and have an orientation-specific resize name.
Their help text explains pointer resize, Alt bypass, and Escape cancellation.
When snapped, help text includes the ratio so the state is not represented only
by mint color. Resize Pane directional commands and Balance Panes provide
keyboard alternatives.

Focused, running, error, and read-only pane states use text or automation state.
High Contrast switches divider feedback to system theme brushes. The snap
interaction does not require animation, does not take terminal focus, and does
not send feedback to the shell.

Narrator, High Contrast, keyboard menus, 100–200% DPI, and mixed-monitor
behavior still require the real Release-build acceptance listed in
[the 1.1 acceptance matrix](../v1.1-acceptance.md).
