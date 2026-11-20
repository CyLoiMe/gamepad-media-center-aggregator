# Brand

Source artwork for the app's identity. Everything under `resources/icon/` that is
not an SVG UI glyph is generated from here — never edit those PNG/JPEG files by
hand, regenerate them.

| File | Role | Derivatives |
|---|---|---|
| `logos/logo.jpg` | Master, 2048×2048: the rounded plate on its dark ground | `resources/icon/icon.png` (1024²), `icon.jpg` (256²), `icon-rounded.png` (512², transparent corners), `logo-ghost.png` (512²) |

## Regenerating

```sh
python scripts/make-icons.py    # needs Pillow
```

The script cuts every derivative from the same plate crop, so they cannot drift
apart. This master is SMCA's own artwork; the backlog item to replace the
upstream icon is closed.

### Constraints

`icon.jpg` is the HOME-menu tile: it goes into the NRO (`elf2nro --icon`) and the
forwarder's `control/icon_AmericanEnglish.dat`. It must stay a **256×256 baseline
JPEG, RGB, no colour profile, no EXIF/XMP** — the HOME menu refuses anything else
for a forwarder and the tile comes up blank with error 2016-1257 (commit 8db17862).
The script writes it with `progressive=False` and strips metadata for that reason.

`icon-rounded.png` is drawn on the theme background (`iconInterpolation`, no
`cornerRadius`), so its corners are real transparency, not a baked-in colour.

## Two icons, two destinations

`icon.jpg` feeds **both** the hbmenu entry (`elf2nro --icon`) and the HOME-menu
tile. The tile is a separate installed title — the forwarder NSP — whose icon
`scripts/forwarder/CMakeLists.txt` copies into `control/icon_AmericanEnglish.dat`
at build time. Replacing `SMCA.nro` therefore does not touch an already installed
tile; it must be reinstalled from a build that regenerated `forwarder.nsp`.
`app/src/utils/forwarder.cpp` verifies that the console kept the new artwork, and
`scripts/inspect-nro.py` prints the NSP a built NRO carries.
