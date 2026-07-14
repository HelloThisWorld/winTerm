# winTerm appearance architecture

## Status and baseline

This document describes the v0.2 source design in the current working tree. It does not certify a build or a running application.

| Item | Recorded value |
| --- | --- |
| Repository baseline | `main@747d6b06a803b482b8d4fb232efc4a6a772f98b1` |
| Microsoft Terminal source baseline | `release-1.25@1cea42d433253d95c4487a3037db48197b5e72f4` |
| Release tag at the repository baseline | None |
| Remotes observed at inspection | `origin` only; no `upstream` remote |
| v0.2 build and runtime status | Not tested |

The appearance classes and assets are source-present and can be reviewed or validated independently. All Appearance C++ sources are registered in the Settings Model static-library project, the Settings editor links that library for the native importer API, and the resource build list contains the winTerm assets. This wiring has been checked structurally but has not been compiled, linked, or runtime-tested in the current environment.

## Ownership boundary

winTerm owns the code under `src/winterm/Appearance` and the manifests under `assets/winterm`. Microsoft Terminal continues to own the terminal engine, ConPTY integration, VT parser, text buffer, renderer, Unicode width calculation, shaping, cell model, and input protocol handling.

The v0.2 boundary is intentionally an adapter layer:

1. A theme or font asset is described by winTerm metadata.
2. A winTerm registry validates and resolves that metadata.
3. A future settings integration can translate the resolved choice into existing Microsoft Terminal settings and renderer inputs.
4. Rendering remains in the upstream DirectWrite and terminal renderer path.

No v0.2 appearance requirement authorizes a rewrite of the renderer core, Unicode width engine, grapheme segmentation, or terminal cell model.

## Theme descriptor

`ThemeDescriptor` separates a complete theme from a Windows Terminal ANSI color scheme. It stores:

- a stable ID, schema version, display name, and variant;
- source, author, revision, and license attribution;
- foreground, background, cursor, selection, ANSI, and bright colors;
- window opacity, acrylic preference, tab colors, and pane border color.

Schema version 1 uses canonical `#RRGGBB` or `#AARRGGBB` output. Import paths can accept additional common hexadecimal forms and normalize them before serialization. A theme must have exactly eight ANSI and eight bright colors. IDs are locale-independent and are not derived from a translated display name.

`cursorTextColor` and `selectionForeground` are currently descriptor metadata only. The existing Windows Terminal color-scheme projection does not expose mappings for them, so the adapter does not claim that they affect rendering. They are retained for round-tripping and a future settings contract.

## Theme registry and preview

`ThemeRegistry` has explicit registration paths for built-in, bundled third-party, and user-imported themes. Protected built-in or bundled ID collisions fail validation. Imported collisions receive a deterministic numeric suffix instead of replacing a protected theme.

Resolution order in the source implementation is:

1. temporary preview;
2. profile theme;
3. global theme;
4. `winterm.midnight` fallback.

`ThemePreviewService` owns the temporary value and clears it on cancel or destruction. `Commit` returns the selected descriptor, but persistence and the Settings UI are separate responsibilities and are not currently runtime-validated.

`ThemeMigration` maps a legacy Windows Terminal scheme name to a registered theme ID when a supplied mapping exists. Otherwise, it returns the safe fallback and an English migration result. It does not yet constitute an end-to-end settings schema migration.

## Importer architecture

`ThemeLoader` selects an importer by file name and, for generic JSON, by the parsed document shape. Dedicated importers handle iTerm2 XML plist, macOS Terminal XML plist, Windows Terminal scheme JSON, and winTerm theme JSON.

The parsers operate on caller-provided content and do not use network access, commands, registry changes, environment expansion, or external file inclusion. Import limits are one MiB per file, 64 JSON or plist nesting levels, 64 KiB per string, 64 imported schemes, and 4,096 plist values. The plist parser handles a constrained XML subset, rejects custom document type declarations and entity expansion, and does not support a binary outer plist.

## Font registry and fallback

The font boundary consists of descriptors, a manifest-backed registry, a merge seam for caller-supplied system descriptors, fallback resolution, OpenType feature settings, and glyph-coverage reporting. A DirectWrite discovery backend and an app-private collection loader are not present in the current source. These components therefore describe policy; DirectWrite remains responsible for eventual font fallback and color-glyph rendering.

The intended family-level fallback order is:

1. selected terminal family when it is present in the supplied registry;
2. Cascadia Mono NF only when it was actually discovered and fallback is enabled;
3. Cascadia Mono, using the discovered spelling or the stable family name;
4. Segoe UI Symbol when general fallback is enabled;
5. Segoe UI Emoji when emoji fallback is enabled;
6. DirectWrite system fallback after the named chain.

Cascadia Code and Cascadia Mono files already inherited in `res/fonts` are pinned in the v0.2 manifest as app-private candidates. The winTerm package mapping places them under `AppearanceAssets/fonts/bundled`, and the registry accepts the checked-in or package-relative manifest paths. The DirectWrite app-private loading backend has not been completed or runtime-tested. Cascadia Mono NF is not bundled, so Nerd Font private-use glyphs require a compatible user-installed font.

## Renderer integration boundary

The theme projection can produce a Windows Terminal color-scheme object containing foreground, background, cursor color, selection background, and the 16 ANSI colors. It does not replace indexed-color, True Color, shaping, emoji, or glyph-run logic.

Color emoji and fallback are expected to continue through the upstream DirectWrite renderer. COLR/CPAL, bitmap, SVG, variation-selector, ZWJ, and regional-indicator behavior must be measured on an actual build. Source reuse is not evidence that every format or sequence passes.

## Upstream isolation

Appearance-specific assets, schemas, import policy, and license gates remain in winTerm-owned paths. Any future project wiring should be narrowly scoped to build registration and settings adapters. Changes to ConPTY, the VT parser, text buffer, renderer core, Unicode width engine, input parser, or OpenConsole internals require a separately documented acceptance blocker and review.

## Validation status

| Capability | Current evidence |
| --- | --- |
| Theme and font manifest structure | Passed `scripts/winterm/validate-assets.ps1` in Windows PowerShell 5.1 on 2026-07-14 |
| Theme descriptor, importer, registry, preview, and migration source | Present for source review |
| Font registry, fallback policy, features, and coverage source | Present for source review; no DirectWrite discovery or app-private loader backend |
| Compilation and linking | Not tested |
| Settings and Theme Gallery integration | Search/import/export adapter and bundled palette projections are wired; full descriptor metadata and live preview are not complete; build/runtime not tested |
| Renderer behavior | Inherited boundary identified; runtime not tested |
| Package inclusion and app-private font loading | Not tested |
