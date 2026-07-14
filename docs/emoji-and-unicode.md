# Emoji, ANSI, and Unicode

## Renderer policy

winTerm v0.2 reuses the Microsoft Terminal text buffer, Unicode width engine, shaping, cell model, and DirectWrite renderer. The appearance layer does not replace them. This reduces fork risk, but reuse alone is not a rendering pass.

The intended emoji path falls back to Segoe UI Emoji and DirectWrite color-glyph drawing. COLR/CPAL, bitmap color glyphs, SVG color glyphs, `sbix`, skin tones, variation selectors, ZWJ sequences, and flags are platform- and font-dependent. Their actual behavior has not been tested in a built winTerm binary.

Emoji must never be converted into permanent bitmap assets by the theme or font layer.

## Fixtures

`scripts/winterm/test-rendering.ps1` writes ANSI 16 colors, the 256-color cube and grayscale ramp, a 24-bit gradient, foreground/background combinations, and the local Unicode fixture. It launches no shell and executes no external program.

`assets/winterm/samples/ansi-test.txt` documents the color checks. `assets/winterm/samples/unicode-test.txt` contains box drawing, blocks, Braille, arrows, mathematics, geometric symbols, Powerline private-use characters, Nerd Font probes, CJK, and the emoji acceptance set.

Fixture output is evidence that test data was emitted, not that winTerm rendered it correctly. Visual results must be recorded separately.

## Manual matrix

Run the rendering script in PowerShell, Command Prompt, and at least one available WSL profile. Record `Pass`, `Fail`, `Partial`, or `Not tested` for each row.

| Area | Required observation | Current result |
| --- | --- | --- |
| ANSI 16 and bright colors | Distinct foreground and background cells; reset works | Not tested |
| ANSI 256 | 6 x 6 x 6 cube and grayscale ramp are stable | Not tested |
| 24-bit color | Gradient is smooth and does not leak formatting | Not tested |
| Bold, dim, reverse | Attributes do not corrupt neighboring cells | Not tested |
| Cursor and selection | Colors remain legible over ANSI output | Not tested |
| Box and block characters | Lines align under a suitable font | Not tested |
| Braille and symbols | Cell width and copying remain stable | Not tested |
| Powerline | Aligns when the selected font covers the glyphs | Not tested |
| Nerd Font | Icons appear only with a compatible installed font | Not tested |
| CJK | Chinese, Japanese, and Korean mix without adjacent-cell damage | Not tested |
| Basic and color emoji | Glyph is visible; color appears where supported | Not tested |
| Skin tone and ZWJ | Modifier or sequence remains visually attached | Not tested |
| Flags, variation selectors, keycaps | Sequence is rendered and copied intact | Not tested |

## Width and cluster limitations

The existing Unicode width engine and terminal cell model remain protected. No claim is made that all emoji sequences occupy a single terminal cell or a single visual cluster on every Windows build. ZWJ behavior is not yet characterized for the v0.2 binary.

Acceptance requires checking that cursor movement and backspace do not split surrogate pairs, selection does not corrupt a cluster, copy preserves the original Unicode sequence, and mixed CJK plus emoji does not overwrite adjacent cells. If a sequence remains limited, document the exact Windows version, font, code points, and observed cell behavior rather than changing the Unicode engine in v0.2.

## Color metadata boundary

ANSI 16, 256, and True Color continue through the upstream VT and renderer paths. The winTerm theme descriptor supplies the base 16-color palette. `cursorTextColor` and `selectionForeground` are stored metadata but are not currently projected into the live Windows Terminal scheme, so their rendering interaction is not an acceptance pass.
