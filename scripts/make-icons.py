#!/usr/bin/env python3
"""Regenerate the app icons and the splash logo from the brand master.

Master: `brand/logos/logo.jpg` (2048x2048, the squircle on its dark ground).
Needs Pillow.

Every derivative is cut from the same crop of the master, so they cannot drift
apart:

| File                              | Size  | Format             | Used for                     |
|-----------------------------------|-------|--------------------|------------------------------|
| `resources/icon/icon.jpg`         | 256²  | baseline JPEG, RGB | HOME tile, NRO icon, NACP    |
| `resources/icon/icon.png`         | 1024² | PNG, RGB           | high-resolution tile         |
| `resources/icon/icon-rounded.png` | 512²  | PNG, RGBA          | in-app logo (sign-in, hints) |
| `resources/icon/logo-ghost.png`   | 512²  | PNG, RGBA          | splash screen logo           |

`icon.jpg` must stay a clean 256x256 baseline JPEG with no metadata: the Switch
HOME menu shows no icon at all for a forwarder whose control icon is anything
else (8db17862, error 2016-1257).

The in-app pair is the same art masked to the plate's outline, so the corners are
transparent for the screens that draw it on the theme background;
`logo-ghost.png` insets it by 21px on the 512 canvas, the margin the splash
screen was laid out against.
"""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "brand/logos/logo.jpg"
OUT = ROOT / "resources/icon"

# The plate's outer rim on the master, measured with gradient peaks, squared off
# so the crop is exactly square. Cutting on the rim means the mask's straight
# edges land on the drawn stroke instead of leaving a sliver of background.
PLATE_BOX = (177, 186, 1871, 1880)
PLATE_RADIUS = 400  # corner arc radius, fitted to the rim profile
SPLASH_MARGIN = 21


def plate_mask(size: tuple[int, int], radius: int) -> Image.Image:
    """Alpha for the rounded plate, drawn 4x and downsampled for clean arcs."""
    scale = 4
    width, height = size
    mask = Image.new("L", (width * scale, height * scale), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, width * scale - 1, height * scale - 1), radius=radius * scale, fill=255
    )
    return mask.resize(size, Image.LANCZOS)


def main() -> None:
    master = Image.open(SRC).convert("RGB")
    if master.size != (2048, 2048):
        raise SystemExit(f"{SRC} is {master.size}, expected 2048x2048")

    plate = master.crop(PLATE_BOX)

    master.resize((1024, 1024), Image.LANCZOS).save(OUT / "icon.png", optimize=True)
    master.resize((256, 256), Image.LANCZOS).save(
        OUT / "icon.jpg", quality=92, optimize=True, progressive=False
    )

    rounded = plate.convert("RGBA")
    rounded.putalpha(plate_mask(plate.size, PLATE_RADIUS))
    rounded = rounded.resize((512, 512), Image.LANCZOS)
    rounded.save(OUT / "icon-rounded.png", optimize=True)

    ghost = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
    inset = 512 - 2 * SPLASH_MARGIN
    ghost.paste(rounded.resize((inset, inset), Image.LANCZOS), (SPLASH_MARGIN, SPLASH_MARGIN))
    ghost.save(OUT / "logo-ghost.png", optimize=True)

    for name in ("icon.jpg", "icon.png", "icon-rounded.png", "logo-ghost.png"):
        path = OUT / name
        image = Image.open(path)
        print(
            f"{name:20s} {image.format:4s} {image.mode:5s} {image.size} "
            f"{path.stat().st_size:>7} B  chunks={sorted(image.info)}"
        )


if __name__ == "__main__":
    main()
