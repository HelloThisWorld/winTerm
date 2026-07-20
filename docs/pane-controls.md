# Pane Controls

winTerm 0.7 defines a compact pane header for every pane in a split layout. The header is outside the terminal buffer, so its pointer input is never sent to the shell.

## Visibility

Pane header visibility supports:

- Automatic: hidden for a one-pane tab and shown for every pane when the tab has two or more panes.
- Always: shown even for a one-pane tab.
- Never: hidden without removing Command Palette, keyboard move mode, or the pane-menu shortcut.

The supported logical height is 24–30 pixels; the default is 26. Showing or hiding a header changes the space offered to the terminal control, which must trigger the normal row and column recalculation.

## Header content

The compact header contains a drag grip, a pane title, focus/status text exposed to accessibility, and an optional overflow button. It does not contain a large toolbar.

Title precedence is:

1. User-defined pane title.
2. Shell-reported title.
3. Profile name.
4. Shell type.

Absolute paths are reduced to their final component before display. Long titles are ellipsized while the accessible pane name remains meaningful.

Focused, running, failed-command, and read-only states have text or automation state and do not rely on color alone.

## Settings

Pane Controls are part of Docking and Layout settings:

- Show Pane Headers
- Pane Header Height
- Show Pane Title
- Show Profile Icon
- Show Overflow Button
- Enable Pane Handle Dragging
- Show Snap Layout Overlay
- Enable Corner Zones
- After Split
- Split Profile

These are view/profile settings. They do not add fields to the Workspace schema.
