# Build

One target: `SMCA.nro`, for Nintendo Switch only. CMake refuses any other platform
with a clear message.

## CI (the normal loop)

`.github/workflows/switch.yaml`, on every push to `main`/`smca`, on PRs and on
manual dispatch:

1. `devkitpro/devkita64:20260219` container;
2. `git -C library/borealis apply ../../scripts/patches/borealis-fixes.patch`
   (**required** — the app relies on those fixes);
3. installs the pinned portlibs from `$DEPS_URL` (a `ci-deps` prerelease);
4. builds libusbhsfs from source (UMS support);
5. `cmake -DPLATFORM_SWITCH=ON -DUSE_LIBUSBHSFS=ON -DBUILTIN_NSP=ON` +
   `make SMCA.nro`;
6. uploads `SMCA-NintendoSwitch[-deko3d]-<run>.zip` and the debug `SMCA.elf`.

Dispatch inputs: `driver` (`deko3d` default, `opengl` fallback) and `no_forwarder`
(skip the built-in NSP forwarder).

## Local

Docker (mirrors CI, no toolchain needed):

```sh
./scripts/build-switch.sh                  # deko3d
DRIVER=opengl ./scripts/build-switch.sh    # OpenGL fallback
```

devkitPro installed locally:

```sh
git clone --recursive <repo> && cd <repo>
git -C library/borealis apply ../../scripts/patches/borealis-fixes.patch
# portlibs: install the exact set listed in switch.yaml (mpv 0.36, ffmpeg 7.1.5, …)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON \
      -DUSE_LIBUSBHSFS=ON -DBUILTIN_NSP=ON
make -C build SMCA.nro -j$(nproc)
```

Output: `build/SMCA.nro`, install as `sdmc:/switch/SMCA.nro`.

## Pinned portlibs

The Switch build is version-sensitive (mpv 0.36 + ffmpeg 7.1.5 + curl 8.16 are the
combination verified on hardware). They are `.pkg.tar.zst` files published in a
`ci-deps` **prerelease**, currently GMCA's (see the `DEPS_URL` TODO in
`switch.yaml`). `scripts/switch/*/PKGBUILD` rebuilds them; the
`switch-portlibs` workflow does that on manual dispatch. `docs/releasing.md`
explains how to host your own set.

## Two different icons

The HOME menu never reads the NRO's icon. `elf2nro --icon` sets the artwork hbmenu
shows for `SMCA.nro`, while the tile is a separate installed title — the forwarder
NSP — whose icon is baked in at build time:

```
resources/icon/icon.jpg
  ├─ elf2nro --icon          → hbmenu entry for SMCA.nro
  └─ scripts/forwarder       → control/icon_AmericanEnglish.dat  (the HOME tile)
       hacbrewpack           → resources/forwarder.nsp  (gitignored)
       elf2nro --romfsdir    → romfs:/forwarder.nsp inside SMCA.nro
       HintActivity          → mini::InstallSD("romfs:/forwarder.nsp")
```

So replacing `SMCA.nro` changes neither an installed tile nor its icon: the tile
only picks up new artwork when it is reinstalled **from a build that regenerated
`forwarder.nsp`**. That target depends on `resources/icon/icon.jpg` and clears its
previous output before packing, so a rebuild does pick up changed artwork — but a
tile installed from an older NRO keeps the old one.

Check what a build actually carries, offline:

```sh
python scripts/inspect-nro.py build/SMCA.nro build/check
```

It prints the NRO's icon size, the NACP name/author/version, and whether the
current commit's short tag is in the binary; `build/check-icon.jpg` is the artwork
to diff against `resources/icon/icon.jpg`. Note what it *cannot* show: the tile
icon itself lives in the forwarder NSP's control NCA, encrypted by hacbrewpack with
the build keyset, so it is not readable offline. The NRO's icon and the tile icon
come from the same `resources/icon/icon.jpg` at build time, so an up-to-date NRO
icon is the available proxy.

To force a clean NSP, delete `resources/forwarder.nsp` (and
`build/resources/forwarder.nsp`) before rebuilding.

If the NRO icon already is the new artwork but the tile still renders the old one,
the build is fine — the HOME menu keeps a per-title-ID icon cache and a reinstall
with the same ID does not always refresh it. Reboot the console (the cache is
rebuilt at boot), then reinstall the tile if it is still stale.

## Verifying on hardware

```sh
nxlink -s build/SMCA.nro     # stdout/stderr + brls logs over the network
```

Logs worth knowing: `MPVCore => EVENT_*` (player lifecycle),
`jellyfin: remote source, playing the server's DirectStreamUrl` (see
`docs/playback.md`), `jellyfin PlaybackInfo …` warnings when the extra request
fails (playback continues on the assembled URL).

## Common failures

| Symptom | Cause |
|---|---|
| `SMCA builds for Nintendo Switch only` | configure without `-DPLATFORM_SWITCH=ON` |
| `Could NOT find MPV` / `pkg_search_module(mpv)` | portlibs missing or `PKG_CONFIG_LIBDIR` not pointing at the Switch ones |
| `patch does not apply` on borealis | submodule not at the pinned commit (`git submodule update --init --recursive`) |
| `404` while fetching `$DEPS_URL` | the `ci-deps` release changed; repoint `DEPS_URL` |
| NRO launches then exits immediately | usually a missing romfs resource — check the `elf2nro --romfsdir` step ran (it needs `resources/`) |
| HOME tile still shows the old icon after updating | the tile icon comes from the embedded forwarder NSP, not the NRO — reinstall the tile from a build that regenerated `forwarder.nsp` (§Two different icons) |

## Tests

```sh
./tests/run.sh          # every tests/test_*.cpp, standalone
```

No CMake wiring: each test is a `main()` compiled with `-Iapp/include`, so anything
under test must be header-only and free of borealis/mpv includes.
