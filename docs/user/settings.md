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

## Appearance → Visual progress

- **Show visual progress** (`visualProgress.enabled`): on by default. Shows the
  per-pane Rainbow Arc Weld overlay for supported progress and shell lifecycle
  state.
- **Recognize command-line progress**
  (`visualProgress.recognizeCliProgress`): on by default. Uses bounded local
  parsers for supported CLI progress formats.
- **Performance mode** (`visualProgress.performanceMode`): `automatic` by
  default; accepted values are `automatic`, `full`, `balanced`, and `minimal`.
  Automatic adapts effects to active progress count and runtime conditions.
- **Replace recognized progress output**
  (`visualProgress.replaceRecognizedOutput`): off by default. When enabled,
  only an unambiguous, high-confidence, single-line transient frame from pip,
  Git, curl, or wget may be replaced; ordinary output and unsupported records
  are always preserved.

Turning off **Show visual progress** disables the other three controls without
changing their saved values. Turning off CLI recognition disables recognized
output replacement without changing that saved preference. Settings apply to
open panes without restarting winTerm.

Set `WINTERM_DISABLE_VISUAL_PROGRESS=1` before launching winTerm for the
authoritative emergency off switch. It disables the renderer, recognizer,
animation, and output replacement regardless of saved settings. With the
override active, terminal output passes through unchanged.

Reduced Motion removes continuous decorative motion, interpolation, breathing,
sweeps, and sparks. High Contrast uses a solid system-compatible presentation;
neither mode invents a percentage for indeterminate progress.

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
