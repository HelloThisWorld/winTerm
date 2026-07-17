# Docking accessibility

## Overlay semantics

Each overlay zone provides:

- A visible icon.
- An optional text label.
- An automation name describing the action.
- A button automation role.
- Enabled, disabled, and selected state in the presentation model.
- A disabled reason appended to the accessible description.

State is not communicated by color alone. Corner zones are omitted when the target geometry is too small. High-contrast preference and 100% to 300% DPI scale are explicit overlay inputs; the runtime renderer must map them to theme resources and target-window DPI.

The overlay is an adorner, not an input focus target. Pointer docking must preserve terminal keyboard focus until a transaction commits. Tab-strip insertion and content overlay highlighting must be mutually exclusive through target priority.

## Preview alternatives

Preview regions are calculated from the proposed layout and classified as moved content, existing content, or an empty layout slot. The preview also exposes a textual summary such as `Preview: topRight docking`. Invalid plans expose their reason instead of geometry.

New-tab and edge/corner actions use distinct accessible names. Disabled operations explain whether the target is closing, read-only, too large, unsupported across processes, or disabled by settings.

## Keyboard and Narrator

The keyboard controller uses the same available zones as the visual overlay and announces the selected target. Enter requests the normal transaction path and Escape cancels without layout mutation.

Runtime acceptance requires checks with Narrator for:

- Source and target names.
- Selected and disabled states.
- Zone changes while using arrow keys.
- Enter and Escape instructions.
- Focus on the moved content after commit.
- Focus restoration after rollback.
- High Contrast themes.
- 100%, 150%, 200%, and 300% scaling.

These runtime checks are not complete in the current environment, so the XAML adapter remains disabled.
