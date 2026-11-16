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
  `PlaybackInfo` — see `EMBY_REMOTE_PLAYBACK.md`.
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

CI (`.github/workflows/switch.yml`) builds `SMCA.nro` on every push — grab the
artifact from the Actions run.

Locally, with [devkitPro](https://devkitpro.org/wiki/Getting_Started) installed:

```sh
git clone --recursive <this repo>
cd <this repo>
git -C library/borealis apply ../../scripts/patches/borealis-fixes.patch

# portlibs: .github/workflows/switch.yml lists the exact pinned set
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON \
      -DUSE_LIBUSBHSFS=ON -DBUILTIN_NSP=ON
make -C build SMCA.nro -j$(nproc)
```

`scripts/build-switch.sh` does the same inside the devkitPro Docker image:
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
| `scripts/switch` | PKGBUILDs of the Switch portlibs this build pins |
| `scripts/patches` | borealis fixes applied at build time |
| `tests/` | standalone logic tests (`./tests/run.sh`, no CMake wiring) |

## License

Apache License 2.0 — see `LICENSE` and `NOTICE`. SMCA is based on GMCA by thcolin;
the original copyright notices are kept.

Bundled third-party code keeps its own license: borealis, lunasvg, miniz, and the
ffmpeg / libmpv builds from devkitPro that the NRO links against.
