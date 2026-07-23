# Pane resize snapping

Snapping helps create clean pane proportions while preserving free resizing.
The default **Common ratios** preset contains:

- 25% — `1 / 4`
- 33.333% — `1 / 3`
- 50% — `50 / 50`
- 66.667% — `2 / 3`
- 75% — `3 / 4`

**Balanced** offers only 50%. **Custom** accepts up to 12 comma-separated
finite ratios between 0.05 and 0.95; values are sorted, duplicates and invalid
entries are removed, and transaction geometry filters anything that violates
the current child minimum sizes.

## Threshold and hysteresis

The divider enters a snap point within 8 logical pixels and remains snapped
until the pointer moves more than 14 logical pixels away. Those values live in
`InteractionTokens`, are evaluated against the immediate split size, and avoid
rapid transitions around a ratio. A configured entry threshold larger than the
default receives a release threshold at least one logical pixel larger.

Hold **Alt** while dragging for free resizing. Alt clears any active snap and
does not send input to the shell.

## Feedback

The active divider uses the mint accent and a compact ratio label. The label is
outside the terminal buffer, does not take focus, causes no layout shift, and
disappears when the resize completes. The divider automation help text reports
the snap ratio, so color is not the only signal. High Contrast uses system
theme brushes. No sound or motion-dependent feedback is used.
