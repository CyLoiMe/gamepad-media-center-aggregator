#!/usr/bin/env python3
"""Inspect a Switch NRO offline: icon, NACP identity, embedded forwarder NSP.

Answers the questions a build artifact can answer without a console:

    python scripts/inspect-nro.py SMCA.nro [out-prefix] [--extract forwarder.nsp]

- extracts the NRO's own icon (the `elf2nro --icon` artwork, what hbmenu shows)
  and reports its size/format so it can be diffed against resources/icon/icon.jpg;
- reads `SMCA.nacp` out of the ASET header: display name, author, version;
- looks for the current commit's short tag in the binary, so "did my latest
  change actually get into this NRO?" has an answer;
- finds the forwarder NSP inside the romfs (it is stored uncompressed) and
  compares it, byte for byte, with `resources/forwarder.nsp` — the file the build
  just packed. That separates "the NRO carries the old tile installer" from "the
  HOME menu kept its old icon", which look identical on the console.

What it CANNOT show: the HOME-menu tile icon itself. That artwork lives in the
forwarder NSP's control NCA, which hacbrewpack encrypts with the build keyset.
See docs/build.md.

NRO layout used here: 0x10-byte NroStart, then a 0x80 NroHeader, then the segments,
with the ASET (icon + NACP) appended at the end:
  magic "ASET", u32 reserved, u64 icon_offset, u64 icon_size,
  u64 nacp_offset, u64 nacp_size       (all offsets relative to the ASET start)
NACP: name[0x200], author[0x200], then version at 0x3060.
PFS0 (the NSP container): "PFS0", u32 file_count, u32 string_table_size, u32
reserved, then file_count x { u32 offset, u32 size, u32 name_offset, u32 reserved }
and the string table; entry offsets are relative to the end of the string table.
"""

import hashlib
import struct
import subprocess
import sys
from pathlib import Path

ASET_MAGIC = b"ASET"
PFS0_MAGIC = b"PFS0"
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


def parse_pfs0(blob: bytes, at: int):
    """Parse the PFS0 archive at `at`: (data_start, total_size, entries, stride).

    `entries` is [(name, offset, size)], offsets relative to `data_start`. Two
    entry strides are in circulation (0x18 with reserved words, 0x10 without), so
    both are tried and the one whose offsets, sizes and name offsets all land
    inside the archive wins — that also rejects a chance "PFS0" in unrelated data.
    `total_size` is the end of the last entry, i.e. the archive without whatever
    padding the packer added; hashing that much makes two builds comparable.
    """
    if blob[at:at + 4] != PFS0_MAGIC:
        return None
    count, strtab = struct.unpack_from("<II", blob, at + 4)
    if not 0 < count <= 64 or strtab > 0x4000:
        return None
    for stride in (0x18, 0x10):
        names_at = at + 0x10 + count * stride
        data_start = names_at + strtab
        if data_start > len(blob):
            continue
        entries, ok, total = [], True, 0
        for i in range(count):
            off, size, name_off = struct.unpack_from("<III", blob, at + 0x10 + i * stride)
            if name_off >= strtab or data_start + off + size > len(blob):
                ok = False
                break
            raw = blob[names_at + name_off:blob.find(b"\0", names_at + name_off)]
            name = raw.decode("ascii", "replace")
            if not name:
                ok = False
                break
            entries.append((name, off, size))
            total = max(total, off + size)
        if ok:
            return data_start, total, entries, stride
    return None


def find_pfs0(blob: bytes):
    """Every parsable PFS0 archive in `blob`, as (offset, ported parse result).

    The romfs keeps files uncompressed, so the forwarder NSP sits in the NRO
    verbatim and stays readable.
    """
    found = []
    at = blob.find(PFS0_MAGIC)
    while at != -1:
        parsed = parse_pfs0(blob, at)
        if parsed:
            found.append((at, parsed))
        at = blob.find(PFS0_MAGIC, at + 4)
    return found


def main() -> int:
    argv = sys.argv[1:]
    extract = None
    if "--extract" in argv:
        i = argv.index("--extract")
        if i + 1 >= len(argv):
            print("--extract needs an output path")
            return 2
        extract = Path(argv[i + 1])
        del argv[i:i + 2]
    if not 1 <= len(argv) <= 2:
        print(__doc__)
        return 2
    src = Path(argv[0])
    prefix = Path(argv[1]) if len(argv) == 2 else None
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

    # The tile installer: the forwarder NSP packed into the romfs. Comparing it
    # with the freshly built one is the only way to tell "this NRO installs an old
    # tile" from "the HOME menu kept the old icon" without a console.
    ref = Path(__file__).resolve().parent.parent / "resources" / "forwarder.nsp"
    nsps = find_pfs0(blob)
    if not nsps:
        print("  embedded NSP: none (built without -DBUILTIN_NSP=ON?)")
    for at, (data, total, entries, stride) in nsps:
        archive = blob[at:data + total]
        looks_nsp = any(n.endswith(".nca") or ".cnmt." in n for n, _, _ in entries)
        print(f"  embedded NSP: 0x{at:x}, {len(entries)} file(s), {len(archive)} B, "
              f"{'NSP contents' if looks_nsp else 'not NSP-shaped'}")
        for name, off, size in entries:
            print(f"    {name}  {size} B")
        print(f"    sha256 {hashlib.sha256(archive).hexdigest()}")
        if not ref.exists():
            print("    resources/forwarder.nsp not present: run this on two builds"
                  " and compare the size/sha256 lines")
            continue
        # Compare the reference's LENGTH of bytes: hacbrewpack may pad the archive,
        # and a padded copy is still the same NSP.
        packed = ref.read_bytes()
        same = len(packed) <= len(archive) and archive[:len(packed)] == packed
        print(f"    resources/forwarder.nsp ({len(packed)} B): "
              f"{'IDENTICAL' if same else 'DIFFERENT — this NRO installs a different tile'}")

    if extract and nsps:
        at, (data, total, entries, stride) = max(nsps, key=lambda x: x[1][1])
        payload = blob[at:data + total]
        extract.write_bytes(payload)
        print(f"\nwrote {extract} ({len(payload)} B): install it with a known-good"
              " installer (DBI, Goldleaf) to test the payload without this app")

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
