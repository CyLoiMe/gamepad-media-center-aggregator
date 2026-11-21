# SMCA — Switch Media Center Aggregator

A controller-first media client for the Nintendo Switch (Atmosphère). Browse a
Plex, Jellyfin, Emby or Stremio library, play it with libmpv, and use the console
like a set-top box: no keyboard, no touch screen, everything reachable with the
gamepad.

SMCA is a fork of [GMCA](https://github.com/thcolin/gamepad-media-center-aggregator)
by thcolin (Apache-2.0), rebuilt as a Switch-only project. See `NOTICE`.

## What it does

- **Servers**: Plex, Jellyfin, Emby, Stremio — one backend interface
  (`app/include/api/backend.hpp`), switchable from the connection screen.
- **Playback**: libmpv, hardware decoding, direct play with automatic transcode
  when needed, resume positions, audio/subtitle track selection, speed, aspect.
- **Remote `.strm` libraries**: items stored on a cloud drive and fronted by a
  302 proxy (TgtoDrive and similar) play through the URL the server hands out in
  `PlaybackInfo`, and are never sent to the transcoder — those front-ends have no
  encoder behind them.
- **Offline**: download to the SD card and play local files, with an offline
  catalog for Jellyfin/Emby sources as well.
- **Files**: browse the SD card, a WebDAV/HTTP share or a USB drive (UMS).
- **UI**: 14 languages, light/dark themes tinted per backend, custom fonts and keymaps.

## Install

1. Copy `SMCA.nro` to `sdmc:/switch/SMCA.nro`.
2. Launch it from hbmenu (or install the built-in forwarder for a HOME tile).
3. Pick a server; settings live in `sdmc:/switch/SMCA/config.json`.

Requires Atmosphère with homebrew support. The default build uses the **deko3d**
renderer; an OpenGL build exists as a fallback.

## Build

**CI is the verified path.** `.github/workflows/switch.yaml` builds `SMCA.nro` on
every push to `main`, and a `v*` tag additionally publishes it as a release. Grab the
artifact from the Actions run. The local steps below mirror that workflow, but they
are best-effort: CI is what gets exercised on every change.

Either way you need [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the
Switch toolchain, plus the pinned portlibs — the container does not ship libmpv,
FFmpeg or curl built *for the Switch*, so the build fails at
`Could NOT find MPV` without them.

### 1. Portlibs

Install the ten packages from this repository's `switch-portlibs` release:

```sh
DEPS_URL=https://github.com/CyLoiMe/switch-media-center-aggregator/releases/download/switch-portlibs

# host-side tool that packs the HOME-tile forwarder
dkp-pacman --noconfirm -U "$DEPS_URL/hacBrewPack-3.05-1-x86_64.pkg.tar.zst"

for pkg in mbedtls-3.6.7-1 libssh2-1.11.1-1 dav1d-1.5.4-1 curl-8.16.0-3 \
           ffmpeg-7.1.5-5 nspmini-main-1; do
  dkp-pacman --noconfirm -U "$DEPS_URL/switch-${pkg}-any.pkg.tar.zst"
done
```

The player library comes in two flavours and they are **not** interchangeable — pick
the one matching the renderer you configure in step 3:

```sh
# deko3d (default): replace the OpenGL libmpv the container already has
dkp-pacman --noconfirm -R switch-libmpv
dkp-pacman --noconfirm -U "$DEPS_URL/switch-libmpv-deko3d-0.36.0-5-any.pkg.tar.zst" \
                          "$DEPS_URL/libuam-master-1-any.pkg.tar.zst"

# OpenGL fallback: leave the packaged libmpv alone, just pin the version
# dkp-pacman --noconfirm -U "$DEPS_URL/switch-libmpv-0.36.0-5-any.pkg.tar.zst"
```

### 2. USB drives (optional, on in CI)

`-DUSE_LIBUSBHSFS=ON` resolves the library with `find_library(... REQUIRED)`, so
configure fails outright when it is missing. Build it first, or drop the flag — the
Files tab then loses USB/UMS browsing:

```sh
git clone https://github.com/DarkMatterCore/libusbhsfs.git --depth=1
make -C libusbhsfs BUILD_TYPE=GPL install -j$(nproc)
```

### 3. Configure and build

```sh
git clone --recursive <this repo>
cd <this repo>
git -C library/borealis apply ../../scripts/patches/borealis-fixes.patch

cmake -B build -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON \
      -DUSE_DEKO3D=ON -DUSE_LIBUSBHSFS=ON -DBUILTIN_NSP=ON
make -C build SMCA.nro -j$(nproc)
```

`-DUSE_DEKO3D=ON` goes with the deko3d libmpv; leave it out for the OpenGL build.
`-DBUILTIN_NSP=ON` embeds the HOME-tile forwarder in the NRO and needs `hacbrewpack`
from step 1; `OFF` builds without it.

`scripts/build-switch.sh` does all three steps inside the devkitPro container:
`./scripts/build-switch.sh` (deko3d) or `DRIVER=opengl ./scripts/build-switch.sh`.

## Configuration

`config.json` (in `sdmc:/switch/SMCA/`) is written by the app; the settings screen
covers the rest. Two files next to it are read at startup:

- `mpv.conf` — any mpv option (filters, cache, `user-agent`, …);
- `font.ttf` / `icon.ttf` — custom UI font and keymap icons.

> **User-Agent**: the app sends exactly one UA — its HTTP client and the player
> share it — because cloud-drive front-ends sign the playback URL against the UA
> of the request that asked for it. Set it in Settings → Network, or with a
> `user-agent=` line in `mpv.conf`.

## Layout

| Path | Contents |
|---|---|
| `app/src`, `app/include` | the app: activities, tabs, views, API backends, utils |
| `resources/` | XML layouts, i18n, fonts, SVG icons, romfs payload |
| `library/borealis`, `library/lunasvg`, `library/miniz` | submodules (UI, SVG, inflate) |
| `brand/` | source artwork; `scripts/make-icons.py` derives `resources/icon/` from it |
| `scripts/switch` | PKGBUILDs of the Switch portlibs this build pins |
| `scripts/patches` | borealis fixes applied at build time |
| `tests/` | standalone logic tests (`./tests/run.sh`, no CMake wiring) |

## Where to start

`app/include/api/backend.hpp` is the backend interface and the neutral `media::*`
models; `app/src/api/{plex,jellyfin,stremio}/` are its three implementations;
`app/src/activity/main_activity.cpp` builds the screens, and
`app/src/activity/player_view.cpp` owns playback. Everything else follows from those.

## License

Apache License 2.0 — see `LICENSE` and `NOTICE`. SMCA is based on GMCA by thcolin;
the original copyright notices are kept.

Bundled third-party code keeps its own license: borealis, lunasvg, miniz, and the
ffmpeg / libmpv builds from devkitPro that the NRO links against.
