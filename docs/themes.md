# winTerm themes

## Current status

The v0.2 working tree contains a schema, registries, import and serialization source, four original winTerm theme assets, and pinned third-party theme assets. The source projects, default-settings projections, and package resource list are wired, while asset validation remains separate from runtime acceptance. No successful build, installed Theme Gallery, settings persistence run, or runtime theme switch is certified by this document.

## Theme classes

| Class | Ownership | Mutability |
| --- | --- | --- |
| Built-in | Original winTerm themes | Protected; cannot be deleted or replaced by an import |
| Bundled | Reviewed third-party themes with pinned source and local license | Protected; cannot be deleted or replaced by an import |
| User-imported | A local file selected by the user | May be renamed, duplicated, exported, or removed by a future UI |

The built-in set is `winterm.midnight`, `winterm.slate`, `winterm.paper`, and `winterm.high-contrast`. The authoritative bundled inventory is `assets/winterm/themes/manifest.json`; documentation must not be used as a substitute for that manifest.

## Built-in palette rationale and contrast

The four winTerm palettes were authored for this repository rather than copied from a third-party theme. Midnight uses a low-glare near-black background with a restrained blue cast. Slate uses neutral charcoal greys to reduce blue tint during long sessions. Paper uses a warm light surface with darker ANSI colors chosen to remain distinguishable on that surface. High Contrast uses black and white anchors, saturated ANSI colors, and an explicit selection pair so cursor and selected text do not rely on a subtle shade change.

The following ratios were calculated from the checked-in sRGB values with the WCAG relative-luminance formula. They cover the primary text, cursor, and selection pairs; they do not replace manual ANSI-palette, high-contrast-mode, or renderer testing.

| Theme | Foreground/background | Cursor/background | Selection foreground/background |
| --- | ---: | ---: | ---: |
| winTerm Midnight | 13.29:1 | 17.04:1 | 9.42:1 |
| winTerm Slate | 10.89:1 | 13.77:1 | 8.69:1 |
| winTerm Paper | 12.24:1 | 7.47:1 | 9.93:1 |
| winTerm High Contrast | 21.00:1 | 15.07:1 | 5.98:1 |

Each theme keeps eight normal and eight bright ANSI entries. The palette values and calculation inputs are reviewable in `assets/winterm/themes/builtin`; actual terminal rendering and every foreground/background combination remain part of manual acceptance.

## Precedence and collision policy

The source registry resolves a theme in this order: temporary preview, profile-specific ID, global ID, then `winterm.midnight`. A missing profile or global ID does not stop startup; resolution continues to the next layer.

Built-in and bundled IDs are globally protected. A duplicate protected ID is an error. A user import that collides receives a suffix such as `.2`; it does not overwrite the existing item. Theme IDs are stable lowercase identifiers and remain independent of display names and localization.

## Schema version 1

A descriptor contains the following required groups:

- `schemaVersion`, `id`, `name`, `displayName`, and `variant`;
- `source.type`, `source.project`, `source.author`, `source.license`, and pinned source metadata where applicable;
- terminal foreground, background, cursor, selection, eight ANSI colors, and eight bright colors;
- window opacity, acrylic preference, tab colors, and pane border color.

Allowed variants are `dark`, `light`, `highContrast`, and `adaptive`. Canonical stored colors are uppercase `#RRGGBB` or `#AARRGGBB`. Opacity is a finite value from `0.0` through `1.0`.

Unknown optional JSON fields are ignored by the current serializer. An unsupported schema version, malformed required field, invalid color, wrong palette length, or out-of-range opacity is rejected.

`cursorTextColor` and `selectionForeground` are preserved in the winTerm descriptor and export format, but are metadata-only in the current Microsoft Terminal scheme projection. They are not claimed to change the live renderer.

## Preview

Preview state is intentionally transient. Starting a preview places a validated descriptor above profile and global choices. Cancel, scope destruction, or a completed commit clears the temporary registry value. A caller must persist the descriptor returned by `Commit`; the source service does not silently write settings.

The existing Color Schemes page has source-level search, import, and export extensions, and bundled palettes are represented in default settings. A full card-based Theme Gallery and integration with `ThemePreviewService` are not complete. The source-level UI changes and an end-to-end cancel/restore interaction have not been build- or runtime-tested in this checkout.

## Migration and storage

The current migration helper maps a known legacy scheme name to a registered theme ID and falls back to `winterm.midnight` when no valid mapping exists. It does not discard the original scheme name.

The target location for imported themes is winTerm-owned application data, logically `%LOCALAPPDATA%\winTerm\themes` when unpackaged or the winTerm MSIX `LocalState` equivalent when packaged. A complete integration should use one normalized JSON file per theme, atomic writes, a rebuildable index, and imported-only deletion. Quarantine, registry persistence, and repeated-migration protection remain acceptance items rather than completed runtime features.

## Safe fallback behavior

A single invalid asset or user file must be skipped or quarantined without preventing launch. The registry fallback is `winterm.midnight`. If that protected asset itself is unavailable, integration code must use an existing Microsoft Terminal safe color scheme and record the failure; this final adapter behavior has not yet been runtime-tested.
