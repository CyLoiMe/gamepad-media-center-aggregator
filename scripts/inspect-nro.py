#!/usr/bin/env python3
"""Inspect a Switch NRO offline: icon, NACP identity, and whether it is current.

Answers the questions a build artifact can answer without a console:

    python scripts/inspect-nro.py SMCA.nro [out-prefix]

- extracts the NRO's own icon (the `elf2nro --icon` artwork, what hbmenu shows)
  and reports its size/format so it can be diffed against resources/icon/icon.jpg;
- reads `SMCA.nacp` out of the ASET header: display name, author, version;
- looks for the current commit's short tag in the binary, so "did my latest
  change actually get into this NRO?" has an answer.

What it CANNOT show: the HOME-menu tile icon. That artwork lives in the forwarder
NSP's control NCA, which hacbrewpack encrypts with the build keyset; it is not
readable offline and not recoverable from this file. See docs/build.md.

NRO layout used here: 0x10-byte NroStart, then a 0x80 NroHeader, then the segments,
with the ASET (icon + NACP) appended at the end:
  magic "ASET", u32 reserved, u64 icon_offset, u64 icon_size,
  u64 nacp_offset, u64 nacp_size       (all offsets relative to the ASET start)
NACP: name[0x200], author[0x200], then version at 0x3060.
"""

import struct
import subprocess
import sys
from pathlib import Path

ASET_MAGIC = b"ASET"
NRO_MAGIC_OFFSET = 0x10
NACP_VERSION_OFFSET = 0x3060


def git_tag(root: Path):
    try:
        out = subprocess.run(
            ["git", "-C", str(root), "rev-parse", "--short", "HEAD"],
            capture_output=True,
            text=True,
            check=True,
        )
        return out.stdout.strip() or None
    except (OSError, subprocess.CalledProcessError):
        return None


def cstr(raw: bytes) -> str:
    return raw.split(b"\0", 1)[0].decode("utf-8", "replace")


def main() -> int:
    if len(sys.argv) not in (2, 3):
        print(__doc__)
        return 2
    src = Path(sys.argv[1])
    prefix = Path(sys.argv[2]) if len(sys.argv) == 3 else None
    blob = src.read_bytes()

    if blob[NRO_MAGIC_OFFSET:NRO_MAGIC_OFFSET + 4] != b"NRO0":
        print(f"{src}: not an NRO (no NRO0 magic at 0x{NRO_MAGIC_OFFSET:x})")
        return 1
    print(f"{src}: {len(blob)} B, NRO0")

    at = blob.find(ASET_MAGIC)
    if at == -1:
        print("  no ASET section: this NRO carries no icon/NACP")
        return 1

    icon_off, icon_size, nacp_off, nacp_size = struct.unpack_from("<QQQQ", blob, at + 8)
    icon = blob[at + icon_off:at + icon_off + icon_size]
    nacp = blob[at + nacp_off:at + nacp_off + nacp_size]

    kind = "JPEG" if icon[:2] == b"\xff\xd8" else "PNG" if icon[:8] == b"\x89PNG\r\n\x1a\n" else "unknown"
    dims = ""
    if kind in ("JPEG", "PNG"):
        try:
            from PIL import Image
            import io

            with Image.open(io.BytesIO(icon)) as im:
                dims = f", {im.size[0]}x{im.size[1]} {im.mode}"
        except Exception:
            dims = ", (install Pillow for dimensions)"
    print(f"  icon: {icon_size} B, {kind}{dims}")

    if nacp_size >= NACP_VERSION_OFFSET + 4:
        print(f"  name  : {cstr(nacp[0:0x200])}")
        print(f"  author: {cstr(nacp[0x200:0x400])}")
        print(f"  version: {cstr(nacp[NACP_VERSION_OFFSET:NACP_VERSION_OFFSET + 0x10])}")
    else:
        print(f"  NACP too small ({nacp_size} B)")

    tag = git_tag(Path(__file__).resolve().parent.parent)
    if tag:
        present = tag.encode() in blob
        print(f"  build tag {tag!r}: {'found in binary' if present else 'NOT found (older build?)'}")

    if prefix:
        icon_path = prefix.with_name(prefix.name + "-icon" + (".jpg" if kind == "JPEG" else ".png"))
        icon_path.write_bytes(icon)
        nacp_path = prefix.with_name(prefix.name + ".nacp")
        nacp_path.write_bytes(nacp)
        print(f"\nwrote {icon_path} and {nacp_path}")
        print("Compare the icon with resources/icon/icon.jpg:")
        print(f'  Get-FileHash {icon_path}, resources/icon/icon.jpg')
    return 0


if __name__ == "__main__":
    sys.exit(main())
