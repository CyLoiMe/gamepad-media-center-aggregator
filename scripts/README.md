# scripts

| Path | What it is |
|---|---|
| `build-switch.sh` | builds `SMCA.nro` in the devkitPro Docker image (mirrors `switch.yaml`); `DRIVER=opengl` for the fallback renderer |
| `switch/` | PKGBUILDs of the Switch portlibs the build pins (mpv, ffmpeg, curl, mbedtls, dav1d, libssh2, libuam, hacbrewpack, nspmini) |
| `patches/` | applied at build time: `borealis-fixes.patch` (required), plus the two v4l2request patches the portlibs use |
| `forwarder/` | the NSP forwarder generator (`-DBUILTIN_NSP=ON`), producing the HOME-menu tile |
| `make-icons.py` | regenerates `resources/icon/{icon.png,icon.jpg,icon-rounded.png,logo-ghost.png}` from `brand/logos/logo.jpg` (needs Pillow) — see `brand/README.md` |
| `inspect-nro.py` | prints a built NRO's icon, NACP name/author/version, and the forwarder NSP it carries (members, size, sha256; `--extract` dumps it) |

Rebuilding the portlibs: run the `switch-portlibs` workflow (manual dispatch); it
publishes to the `switch-portlibs` prerelease that `DEPS_URL` points at. Note that
`scripts/switch/*` currently pins ffmpeg 7.1.4 / mbedtls 3.6.5 / dav1d 1.5.3 while
the build installs 7.1.5 / 3.6.7 / 1.5.4, and the devkitPro port patches are
version-locked — a rebuild therefore does not yet reproduce the mirrored set.
