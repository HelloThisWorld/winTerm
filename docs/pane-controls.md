# Pane controls

winTerm 1.1 uses a compact informational header for every pane in a visible
split layout. It is outside the terminal buffer, so pointer input on the header
is not sent to the shell.

The header contains a pane/profile icon, ellipsized title, real
active/read-only/running status, and overflow button. Clicking the icon focuses
the pane. Right-clicking the icon or header can open the same unified pane menu
as the overflow button. No header control initiates pane repositioning or uses
a movement cursor.

Settings → Appearance → Application UI controls compact/comfortable density,
pane-header visibility, profile icon, active status, and animations. Hiding
headers does not remove directed split, resize, balance, undo/redo, Command
Palette, or keyboard access.

The pane title prefers shell-reported title, profile name, and finally
`Terminal`. Displayed paths are reduced to their final component, long text is
ellipsized, and the full accessible label remains **_title_ pane**.
