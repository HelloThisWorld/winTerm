# Copyright (c) winTerm contributors.
# Licensed under the MIT license.

from pathlib import Path
import re
import xml.etree.ElementTree as ElementTree

from PIL import Image, ImageDraw


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
SOURCE_DIRECTORY = REPOSITORY_ROOT / "assets" / "winterm" / "icons"
PACKAGE_DIRECTORY = REPOSITORY_ROOT / "res" / "terminal" / "images-WinTerm"
SOURCE_SVG = SOURCE_DIRECTORY / "winterm.svg"
ICON_SIZES = (16, 20, 24, 32, 40, 44, 48, 64, 96, 128, 150, 256, 310)
ICO_SIZES = (16, 20, 24, 32, 40, 48, 64, 96, 256)
SVG_NAMESPACE = "{http://www.w3.org/2000/svg}"


def _contrast_palette(variant: str) -> tuple[str, str] | None:
    if variant == "contrast-black":
        return ("#FFFFFF", "#000000")
    if variant == "contrast-white":
        return ("#000000", "#FFFFFF")
    return None


def _number(element: ElementTree.Element, attribute: str, default: float | None = None) -> float:
    raw_value = element.get(attribute)
    if raw_value is None:
        if default is None:
            raise ValueError(f"{element.tag} must define {attribute}.")
        return default
    return float(raw_value)


def _parse_path_points(path_data: str) -> list[tuple[float, float]]:
    tokens = re.findall(r"[MLHV]|-?(?:\d+(?:\.\d*)?|\.\d+)", path_data)
    points: list[tuple[float, float]] = []
    current = (0.0, 0.0)
    index = 0
    while index < len(tokens):
        command = tokens[index]
        index += 1
        if command in {"M", "L"}:
            if index + 1 >= len(tokens):
                raise ValueError(f"Incomplete {command} command in SVG path: {path_data}")
            current = (float(tokens[index]), float(tokens[index + 1]))
            index += 2
        elif command == "H":
            if index >= len(tokens):
                raise ValueError(f"Incomplete H command in SVG path: {path_data}")
            current = (float(tokens[index]), current[1])
            index += 1
        elif command == "V":
            if index >= len(tokens):
                raise ValueError(f"Incomplete V command in SVG path: {path_data}")
            current = (current[0], float(tokens[index]))
            index += 1
        else:
            raise ValueError(
                f"Unsupported SVG path command {command!r}; use only M, L, H, and V."
            )
        points.append(current)
    if len(points) < 2:
        raise ValueError(f"SVG path must contain at least two points: {path_data}")
    return points


def _load_svg() -> tuple[float, list[ElementTree.Element]]:
    document = ElementTree.parse(SOURCE_SVG)
    root = document.getroot()
    view_box = [float(value) for value in root.get("viewBox", "").split()]
    if view_box != [0.0, 0.0, 512.0, 512.0]:
        raise ValueError("winterm.svg must keep the canonical 0 0 512 512 viewBox.")
    shapes = [
        element
        for element in root
        if element.tag in {f"{SVG_NAMESPACE}rect", f"{SVG_NAMESPACE}path"}
    ]
    if not shapes or shapes[0].tag != f"{SVG_NAMESPACE}rect":
        raise ValueError("winterm.svg must begin with its background rect.")
    return view_box[2], shapes


def _render_square(
    size: int,
    source: tuple[float, list[ElementTree.Element]],
    variant: str = "normal",
) -> Image.Image:
    scale = 4
    canvas_size = size * scale
    image = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    view_box_size, shapes = source
    contrast = _contrast_palette(variant)

    def point(value: float) -> float:
        return value * canvas_size / view_box_size

    for shape_index, element in enumerate(shapes):
        foreground = contrast[1] if contrast else None
        if element.tag == f"{SVG_NAMESPACE}rect":
            x = point(_number(element, "x"))
            y = point(_number(element, "y"))
            width = point(_number(element, "width"))
            height = point(_number(element, "height"))
            radius = point(_number(element, "rx", 0))
            fill = contrast[0] if contrast and shape_index == 0 else foreground
            fill = fill or element.get("fill")
            stroke = foreground or element.get("stroke")
            stroke_width = point(_number(element, "stroke-width", 0))

            if stroke and stroke_width > 0:
                half_stroke = stroke_width / 2
                draw.rounded_rectangle(
                    (
                        x - half_stroke,
                        y - half_stroke,
                        x + width + half_stroke,
                        y + height + half_stroke,
                    ),
                    radius=radius + half_stroke,
                    fill=stroke,
                )
                draw.rounded_rectangle(
                    (
                        x + half_stroke,
                        y + half_stroke,
                        x + width - half_stroke,
                        y + height - half_stroke,
                    ),
                    radius=max(0, radius - half_stroke),
                    fill=fill,
                )
            else:
                draw.rounded_rectangle(
                    (x, y, x + width, y + height),
                    radius=radius,
                    fill=fill,
                )
        else:
            points = [
                (point(x), point(y))
                for x, y in _parse_path_points(element.get("d", ""))
            ]
            stroke = foreground or element.get("stroke")
            stroke_width = max(1, round(point(_number(element, "stroke-width"))))
            draw.line(points, fill=stroke, width=stroke_width, joint="curve")
            if element.get("stroke-linecap") == "round":
                cap_radius = stroke_width / 2
                for x, y in (points[0], points[-1]):
                    draw.ellipse(
                        (
                            x - cap_radius,
                            y - cap_radius,
                            x + cap_radius,
                            y + cap_radius,
                        ),
                        fill=stroke,
                    )
            if element.get("stroke-linejoin") == "round":
                join_radius = stroke_width / 2
                for x, y in points[1:-1]:
                    draw.ellipse(
                        (
                            x - join_radius,
                            y - join_radius,
                            x + join_radius,
                            y + join_radius,
                        ),
                        fill=stroke,
                    )
    return image.resize((size, size), Image.Resampling.LANCZOS)


def _render_tile(
    width: int,
    height: int,
    source: tuple[float, list[ElementTree.Element]],
) -> Image.Image:
    image = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    side = min(width, height)
    icon = _render_square(side, source)
    image.alpha_composite(icon, ((width - side) // 2, (height - side) // 2))
    return image


def _write_ico(
    path: Path,
    source: tuple[float, list[ElementTree.Element]],
    variant: str = "normal",
) -> None:
    image = _render_square(256, source, variant)
    image.save(path, format="ICO", sizes=[(size, size) for size in ICO_SIZES])


def main() -> None:
    SOURCE_DIRECTORY.mkdir(parents=True, exist_ok=True)
    PACKAGE_DIRECTORY.mkdir(parents=True, exist_ok=True)
    source = _load_svg()

    for size in ICON_SIZES:
        _render_square(size, source).save(SOURCE_DIRECTORY / f"winterm-{size}.png")
    _write_ico(SOURCE_DIRECTORY / "winterm.ico", source)

    package_tiles = {
        "StoreLogo.png": (50, 50),
        "Square44x44Logo.png": (44, 44),
        "Square150x150Logo.png": (150, 150),
        "SmallTile.png": (71, 71),
        "Wide310x150Logo.png": (310, 150),
        "LargeTile.png": (310, 310),
    }
    for filename, dimensions in package_tiles.items():
        _render_tile(*dimensions, source).save(PACKAGE_DIRECTORY / filename)

    _write_ico(PACKAGE_DIRECTORY / "terminal.ico", source)
    _write_ico(PACKAGE_DIRECTORY / "terminal_contrast-black.ico", source, "contrast-black")
    _write_ico(PACKAGE_DIRECTORY / "terminal_contrast-white.ico", source, "contrast-white")


if __name__ == "__main__":
    main()
