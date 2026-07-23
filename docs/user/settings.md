# Settings

## Appearance → Application UI

- **Application UI theme**: use the existing application theme selector.
- **Density**: Compact uses the website-aligned 27-pixel pane header;
  Comfortable adds spacing.
- **Pane header visibility**: show headers in split layouts.
- **Show pane profile icon**: show the focusable pane/shell icon.
- **Show active status**: show focused, read-only, error, and running state.
- **UI animations**: uses the existing animation preference.

These options style the native application shell. Profile terminal colors,
fonts, opacity, cursor, and background settings remain independent.

## Docking and layout → Pane resizing

- **Enable resize snapping**: on by default.
- **Snap points**: Balanced, Common ratios, or Custom.
- **Custom ratios**: up to 12 comma-separated finite values from 0.05 to 0.95.
- **Show snap ratio indicator**: on by default.
- **Alt disables snapping**: on by default.
- **Snap threshold**: advanced logical-pixel entry distance; default 8.
- **Reset pane resizing to defaults**: restores Common ratios and the standard
  interaction values.

Custom values are sorted and deduplicated. Values that violate the current
pane minimum sizes are ignored for that resize transaction. The release
threshold remains larger than the entry threshold to prevent flicker.

Obsolete pane-movement settings are ignored and are not shown.
