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

## Tests

```sh
./tests/run.sh          # every tests/test_*.cpp, standalone
```

No CMake wiring: each test is a `main()` compiled with `-Iapp/include`, so anything
under test must be header-only and free of borealis/mpv includes.
