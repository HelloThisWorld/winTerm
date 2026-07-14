# Fonts

## Current inventory and status

The v0.2 font manifest pins four existing upstream font files: regular and italic variable-weight files for Cascadia Code and Cascadia Mono, version `2407.24`. The files remain under `res/fonts`; they are not newly downloaded or globally installed by winTerm.

The manifest and registry source establish an app-private loading plan. A winTerm-branded package maps the pinned files to `AppearanceAssets/fonts/bundled` instead of declaring shared system fonts; the manifest records both repository and package-relative paths, and the registry can resolve either layout. The registry can merge system descriptors supplied by a caller, but this checkout does not contain a DirectWrite system-discovery backend or a process-private DirectWrite collection backend. The source therefore does not prove that the package was built, that DirectWrite registers the files for the process, or that Settings can select them. Build, package, launch, and glyph rendering are not tested in the current environment.

Cascadia Mono NF is not bundled. The fallback design must not refer to it as an available bundled file. Nerd Font and Powerline private-use glyphs require a compatible user-installed font unless a separately licensed asset is added through the documented asset process.

## Font sources

| Source | Policy |
| --- | --- |
| App-private | Load a manifest-pinned file for the winTerm process only; never install it globally |
| System | Enumerate DirectWrite font families without copying or modifying them |
| User-selected | Resolve by stable family name and fall back safely when unavailable |

Every app-private entry requires a local file, SHA-256, source project, immutable source revision, source file, version, author, license identifier, and local license text. The authoritative inventory is `assets/winterm/fonts/manifest.json`.

## App-private loading

The intended integration registers packaged Cascadia Code and Cascadia Mono resources with a process-scoped DirectWrite font collection. It must not call a system font installer or write to the Windows Fonts directory. Registration failures must be isolated and must fall back to a usable system family without blocking application launch.

App-private registration and package resource paths are not runtime-validated. The package script is prepared to verify the packaged paths and SHA-256 values after unpacking, but no package exists in this environment. The manifest alone is not evidence that a font was loaded.

## Fallback chain

Fallback should preserve the existing Microsoft Terminal and DirectWrite behavior. The policy order is:

1. selected terminal family when present in the supplied registry;
2. Cascadia Mono NF only when actually discovered and fallback is enabled;
3. Cascadia Mono, using a discovered entry or the stable family name;
4. Segoe UI Symbol when general fallback is enabled;
5. Segoe UI Emoji when emoji fallback is enabled;
6. DirectWrite system fallback after the named chain.

The exact glyph-level fallback can differ by script, Windows version, and installed fonts. CJK, Arabic, Hebrew, Cyrillic, mathematics, box drawing, Braille, Powerline, Nerd Font private-use glyphs, and emoji require runtime coverage checks. A family name appearing in a registry is not a coverage guarantee. Consolas is not inserted by the current resolver; DirectWrite may still choose it as a system fallback.

## Ligatures and features

Cascadia Code is marked as ligature-capable; Cascadia Mono is marked as non-ligature in the manifest. Settings may expose ligatures, contextual alternates, weight, size, line-height, and cell-width adjustment only through existing DirectWrite and Microsoft Terminal feature surfaces.

Disabling ligatures should render `=>`, `!=`, `===`, and `->` as separate glyph choices. Enabling them must not change cursor positions, selection, copied text, or cell alignment. These behaviors require an actual renderer test and are currently not certified.

## Nerd Font and Powerline

The font registry may report a coverage estimate based on inspected code points, but it must label the result as an estimate. winTerm does not bundle Cascadia Mono NF or another Nerd Font in v0.2. Powerline and Nerd Font fixtures are included so a user-installed compatible font can be tested; missing private-use glyphs are an expected result with the bundled Cascadia files.

## Licensing

Cascadia Code and Cascadia Mono are pinned to the local SIL Open Font License 1.1 text. Any future font addition must pass `scripts/winterm/validate-assets.ps1` and update the generated third-party notices. Apple fonts, unclear licenses, and runtime downloads are prohibited.
