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

Output: `build/SMCA.nro`.

## Installing: two paths, and the stale-binary trap

The NRO can live in **two** places, and the one you launch is the one that runs:

| Entry point | File |
|---|---|
| hbmenu | `sdmc:/switch/SMCA.nro` |
| HOME menu tile (forwarder NSP) | `sdmc:/switch/SMCA/SMCA.nro` |
| in-app updater | whatever `argv[0]` was, i.e. `AppVersion::nro_path` |

Copying the new build to only one of them means the other entry point keeps
running the **old binary** — with the old feature set, and no error anywhere. This
already cost a debugging session ("the new features are gone"). Copy it to both,
or delete the copy you no longer use.

To tell which binary is running, look at the settings screen: it prints
`v<version>-<commit> (<platform>)` (`tab/setting_tab.cpp`), and the commit is the
short sha CMake took from git at configure time. Compare it with
`git rev-parse --short HEAD`.

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

It prints the NRO's icon size, the NACP name/author/version, whether the current
commit's short tag is in the binary, and — the useful part for a tile question —
what the forwarder NSP **inside the NRO's romfs** contains:

```
  embedded NSP: 0x1ac160f, 3 file(s), 1184776 B, NSP contents
    0500534d43410000.cnmt.nca  1234 B
    0500534d43410000.cnmt.xml  512 B
    0500534d43410000.nca       1183030 B
    sha256 3f9c…
    resources/forwarder.nsp (1184776 B): IDENTICAL
```

`IDENTICAL` means the NRO carries a current tile installer, so a stale tile is a
console-side problem (§A tile with the old icon). `DIFFERENT` means the NRO you are
running installs an old tile — either it is an older build than
`resources/forwarder.nsp`, or the binary you launch is not the one you built.

When there is no `resources/forwarder.nsp` to compare with (builds made in CI, or a
cleaned tree), run the tool on two NROs instead and compare the **size and sha256**
lines: same values mean the payload never changed, so the build is not the variable.

`--extract forwarder.nsp` writes the archive out. Install that file with a
known-good installer (DBI, Goldleaf) and look at the tile: if it shows the right
icon, the payload is fine and the app's own install is what to look at; if it does
not, the build packed the wrong artwork — the two answers need opposite fixes, and
this is the only way to separate them without trusting the app under suspicion.

`build/check-icon.jpg` is the NRO's own artwork, to diff against
`resources/icon/icon.jpg`. Note what the tool *cannot* show: the tile icon itself
lives in the forwarder NSP's control NCA, encrypted by hacbrewpack with the build
keyset, so it is not readable offline. The NRO icon and the tile icon come from the
same `resources/icon/icon.jpg` at build time, so an up-to-date NRO icon plus a
matching NSP is the available proof.

To force a clean NSP, delete `resources/forwarder.nsp` (and
`build/resources/forwarder.nsp`) before rebuilding.

### A tile with the old icon

A tile is a separate installed TITLE, so its artwork is decided by the control NCA
the console ended up with — not by the NRO you build. Two mechanisms produced the
"old icon for ever, even after delete and reinstall" report, and both are handled in
code now rather than diagnosed by hand.

**1. The payload.** `resources/forwarder.nsp` is packed from
`resources/icon/icon.jpg` at build time. `scripts/inspect-nro.py` prints the NSP
inside a built NRO (members, size, sha256) and can `--extract` it, so what a given
NRO would install is a number instead of a guess; installing that file with DBI or
Goldleaf tests the payload without the app in the way.

**2. The version.** The HOME menu keys its icon cache on the title's version, and
every build used to be stamped `0.1.0` — so a reinstall could look like "same title,
nothing changed" and keep the first artwork. The forwarder NACP now carries the git
build count (`0.1.<build mod 256>`, `scripts/forwarder/CMakeLists.txt`), so each
build is a version the console has not seen before.

**3. The install no longer trusts the write.** `HintActivity` used to call
`mini::InstallSD()` and call it done — and even with its return value checked, "the
write happened" says nothing about what was written. `forwarder::install()`
(`utils/forwarder.cpp`) now reads the icon THIS build carries out of the running
NRO's ASET section (the same artwork `elf2nro --icon` gave hbmenu), installs, then
reads the installed title's control data back with `nsGetApplicationControlData()`
and compares the two. The dialog follows the verdict:

| Outcome | Dialog | Meaning |
|---|---|---|
| `Failed` | `main/hints/install_failed` | nothing was written (SD space, ncm refused) |
| `IconStale` | `main/hints/icon_stale` | written, but the console kept other artwork — said out loud instead of launching a tile the user will find wrong again |
| `Installed` | `main/hints/remove_old` | written and verified; the tile is launched |

A tile from an older title ID is a different title and none of this touches it:
pleNx `0104474D43410000`, GMCA `0500474D43410000`, SMCA `0500534D43410000`. System
Settings → Data Management → Software lists them all.

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
