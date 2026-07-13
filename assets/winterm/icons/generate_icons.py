# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

from pathlib import Path

from PIL import Image, ImageDraw


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
SOURCE_DIRECTORY = REPOSITORY_ROOT / "assets" / "winterm" / "icons"
PACKAGE_DIRECTORY = REPOSITORY_ROOT / "res" / "terminal" / "images-WinTerm"
ICON_SIZES = (16, 20, 24, 32, 40, 44, 48, 64, 96, 128, 150, 256, 310)
ICO_SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 256)


def _palette(variant: str) -> tuple[str, str, str, str, str]:
    if variant == "contrast-black":
        return ("#FFFFFF", "#000000", "#000000", "#000000", "#000000")
    if variant == "contrast-white":
        return ("#000000", "#FFFFFF", "#FFFFFF", "#FFFFFF", "#FFFFFF")
    return ("#0B1020", "#23314D", "#F59E0B", "#5EEAD4", "#F8FAFC")


def _render_square(size: int, variant: str = "normal") -> Image.Image:
    scale = 4
    canvas_size = size * scale
    image = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    background, border, rail, prompt, underscore = _palette(variant)

    def point(value: float) -> int:
        return round(value * canvas_size / 512)

    draw.rounded_rectangle(
        (point(16), point(16), point(496), point(496)),
        radius=point(104),
        fill=background,
        outline=border,
        width=max(1, point(16)),
    )
    draw.rounded_rectangle(
        (point(72), point(128), point(92), point(384)),
        radius=max(1, point(10)),
        fill=rail,
    )
    draw.line(
        ((point(140), point(168)), (point(240), point(256)), (point(140), point(344))),
        fill=prompt,
        width=max(1, point(32)),
        joint="curve",
    )
    draw.line(
        ((point(278), point(344)), (point(398), point(344))),
        fill=underscore,
        width=max(1, point(32)),
    )
    return image.resize((size, size), Image.Resampling.LANCZOS)


def _render_tile(width: int, height: int) -> Image.Image:
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    side = min(width, height)
    icon = _render_square(side)
    image.alpha_composite(icon, ((width - side) // 2, (height - side) // 2))
    return image


def _write_ico(path: Path, variant: str = "normal") -> None:
    image = _render_square(256, variant)
    image.save(path, format="ICO", sizes=[(size, size) for size in ICO_SIZES])


def main() -> None:
    SOURCE_DIRECTORY.mkdir(parents=True, exist_ok=True)
    PACKAGE_DIRECTORY.mkdir(parents=True, exist_ok=True)

    for size in ICON_SIZES:
        _render_square(size).save(SOURCE_DIRECTORY / f"winterm-{size}.png")
    _write_ico(SOURCE_DIRECTORY / "winterm.ico")

    package_tiles = {
        "StoreLogo.png": (50, 50),
        "Square44x44Logo.png": (44, 44),
        "Square150x150Logo.png": (150, 150),
        "SmallTile.png": (71, 71),
        "Wide310x150Logo.png": (310, 150),
        "LargeTile.png": (310, 310),
    }
    for filename, dimensions in package_tiles.items():
        _render_tile(*dimensions).save(PACKAGE_DIRECTORY / filename)

    _write_ico(PACKAGE_DIRECTORY / "terminal.ico")
    _write_ico(PACKAGE_DIRECTORY / "terminal_contrast-black.ico", "contrast-black")
    _write_ico(PACKAGE_DIRECTORY / "terminal_contrast-white.ico", "contrast-white")


if __name__ == "__main__":
    main()
