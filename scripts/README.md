# scripts

| Path | What it is |
|---|---|
| `build-switch.sh` | builds `SMCA.nro` in the devkitPro Docker image (mirrors `switch.yaml`); `DRIVER=opengl` for the fallback renderer |
| `switch/` | PKGBUILDs of the Switch portlibs the build pins (mpv, ffmpeg, curl, mbedtls, dav1d, libssh2, libuam, hacbrewpack, nspmini) |
| `patches/` | applied at build time: `borealis-fixes.patch` (required), plus the two v4l2request patches the portlibs use |
| `forwarder/` | the NSP forwarder generator (`-DBUILTIN_NSP=ON`), producing the HOME-menu tile |
| `make-icons.py` | regenerates `resources/icon/{icon.png,icon.jpg,icon-rounded.png,logo-ghost.png}` from `brand/logos/logo.jpg` (needs Pillow) — see `brand/README.md` |
| `inspect-nro.py` | prints a built NRO's icon, NACP name/author/version, and the forwarder NSP it carries (members, size, sha256; `--extract` dumps it) |

Rebuilding the portlibs: run the `switch-portlibs` workflow (manual dispatch) and
publish the resulting packages as the `ci-deps` prerelease that `DEPS_URL` points at.
