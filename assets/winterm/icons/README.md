# winTerm placeholder icons

## Source format

`winterm.svg` is the canonical vector source. `generate_icons.py` parses its geometry and color values directly for deterministic raster generation; it does not keep a second hand-drawn copy of the normal icon.

## Generated sizes

The source asset set contains PNG files at 16, 20, 24, 32, 40, 44, 48, 64, 96, 128, 150, 256, and 310 pixels, plus a multi-resolution `winterm.ico`. Package-ready tile images and high-contrast executable icons are written to `res/terminal/images-WinTerm`.

## Regeneration

1. Install Python 3 and Pillow in an isolated development environment.
2. Update `winterm.svg`. Keep its intentionally small supported shape vocabulary documented by generator validation.
3. Run `python assets/winterm/icons/generate_icons.py` from any directory.
4. Review every generated size, especially 16, 20, 24, 32, 44, 48, 150, and 310 pixels.
5. Run `scripts/winterm/verify-branding.ps1` before committing the replacement.

Pillow is used only to regenerate committed artwork and is not an application or build dependency.

## Licensing status

The placeholder geometry was created for winTerm and is licensed under the repository MIT license. It contains no third-party logo, font, theme, or downloaded artwork.
