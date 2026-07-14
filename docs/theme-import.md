# Theme import and export

## Supported source formats

| Format | Implemented source behavior | Current limitation |
| --- | --- | --- |
| iTerm2 `.itermcolors` | Reads XML plist ANSI 0-15, foreground, background, cursor, cursor text, selection background, and selected text | Binary outer plist is rejected |
| macOS Terminal `.terminal` | Reads common XML plist ANSI, foreground, background, cursor, selection, and numeric background opacity keys | Binary outer plist is rejected; profile behavior is not imported |
| Windows Terminal JSON | Reads one scheme, an array of schemes, or a top-level `schemes` array | Only color-scheme fields are imported |
| winTerm JSON | Reads and validates schema version 1 and can serialize it with atomic file replacement | UI import/export flow is not runtime-tested |

Generic `.json` is inspected after strict parsing. A document with `schemaVersion` and `terminal` is treated as a winTerm theme; other JSON is treated as Windows Terminal color-scheme input. `.winterm-theme.json` always selects the winTerm importer.

These claims describe source behavior. The importer files have not been compiled or exercised through a running Settings UI in the current environment.

## iTerm2 fields

The importer recognizes `Ansi 0 Color` through `Ansi 15 Color`, `Foreground Color`, `Background Color`, `Cursor Color`, `Cursor Text Color`, `Selection Color`, and `Selected Text Color`. Missing palette entries use the documented safe palette and produce a warning. `Bold Color` is reported as ignored because bold behavior remains tied to the renderer and ANSI palette.

Plist colors may be component dictionaries, supported hexadecimal strings, or recognized archived color data embedded as base64 in an XML plist. Support for embedded color data does not mean that a binary outer plist is supported.

## macOS Terminal fields

Common keys such as `ANSIRedColor`, `TextColor`, `BackgroundColor`, `CursorColor`, and `SelectionColor` are imported, including known alternate `*Colour` spellings. `BackgroundAlpha` or `BackgroundOpacity` is used when numeric.

Startup commands, shell commands, environment variables, keyboard mappings, window behavior, and other macOS-specific settings are ignored. They are never executed or copied into a profile. Cursor text and selection foreground are assigned safe metadata defaults when the source profile does not express them.

## Windows Terminal fields

The importer requires a scheme `name`, foreground, background, and all 16 named ANSI colors. Cursor color and selection background are optional and receive safe fallbacks. Profiles, actions, keybindings, command lines, and `defaultProfile` are reported as ignored.

A Windows Terminal color scheme has no winTerm window, tab, pane, cursor-text, or selection-foreground contract. Those fields receive derived defaults. In particular, `cursorTextColor` and `selectionForeground` remain descriptor metadata and are not mapped back into the current renderer scheme surface.

## winTerm fields and export

winTerm JSON retains theme identity, attribution, terminal colors, and window metadata. Export uses normalized JSON and an atomic file write. Attribution can be omitted by an explicit serializer call, but bundled assets and notices must retain their separate manifest records.

User-imported IDs are generated from a sanitized file name plus a content-derived suffix. A registry collision receives a numeric suffix. The current implementation is deterministic but is not a cryptographic content identity.

## Security restrictions

Import is local and must not perform network access, script execution, command execution, external file inclusion, environment expansion, registry modification, or arbitrary writes. The current source limits input to:

- 1 MiB per file;
- 64 nesting levels;
- 64 KiB per string;
- 64 schemes per JSON import;
- 4,096 values per plist.

JSON parsing rejects comments, duplicate keys, trailing data, single quotes, special floating-point values, and malformed roots. The constrained plist parser does not resolve external entities or remote schemas. Custom document type declarations and unknown entity expansion are rejected. Only the standard Apple plist declaration is tolerated; it is not fetched.

The parser accepts XML outer plists only. A file beginning with `bplist00` returns an English error instructing the user to export XML. This limitation applies to both `.itermcolors` and `.terminal` files.

Only the leaf name of an imported file is retained in theme metadata. A complete UI must avoid displaying exception stacks and must not send full local paths to telemetry.
