# AGENTS.md

Entry point for anyone — human or AI — picking this repository up. Read this page
first, then open the **one** document under `docs/` that matches the task. Nothing
else is required reading.

SMCA (Switch Media Center Aggregator) is a Nintendo Switch (Atmosphère) client for
Plex, Jellyfin, Emby and Stremio. C++17, borealis UI, libmpv playback, one build
target: `SMCA.nro`. It is a Switch-only fork of
[GMCA](https://github.com/thcolin/gamepad-media-center-aggregator) (Apache-2.0 —
see `NOTICE` for the attribution that must stay).

## Where to look

| Document | Read it when you… |
|---|---|
| `docs/architecture.md` | need the code map, the backend interface, or how a screen is put together |
| `docs/build.md` | build, touch CMake/CI, or bump a Switch portlib |
| `docs/playback.md` | touch anything that ends in libmpv: `resolvePlayback`, User-Agent, remote `.strm` sources |
| `docs/conventions.md` | write code, commits, tests or translations |
| `docs/backlog.md` | want to know what is knowingly unfinished |
| `docs/releasing.md` | cut a release |
| `docs/spec.md`, `docs/multi-backend.md` | need the upstream design docs (French, GMCA-era; intent still valid) |
| `docs/upstream/` | the report + PR text of the remote-playback fix, to send to GMCA |

## Fast facts

- **Build**: `cmake -B build -DPLATFORM_SWITCH=ON -DUSE_LIBUSBHSFS=ON -DBUILTIN_NSP=ON`
  then `make -C build SMCA.nro`, inside the devkitPro image. CI
  (`.github/workflows/switch.yaml`) does exactly this — pushing and downloading the
  artifact is the normal loop, no local toolchain required.
- **Tests**: `./tests/run.sh` — standalone, no CMake wiring, needs a POSIX shell and
  a C++17 compiler.
- **Two singletons carry almost everything**: `AppConfig::instance()` (servers,
  settings, the active `media::Backend`) and `MPVCore::instance()` (the player,
  created lazily at the first playback).
- **Icons and brand art are generated**: `brand/logos/logo.jpg` →
  `scripts/make-icons.py` → `resources/icon/` (`brand/README.md`).
- **The UI never formats provider URLs**: everything goes through `media::Backend`
  (`app/include/api/backend.hpp`) and its neutral `media::*` models.
- **One platform**: there is no PS Vita/PS4/desktop/Android code left. Do not add
  `#ifdef` branches for them back; CMake refuses any platform but `PLATFORM_SWITCH`.
- **Every user-visible string is translated** in `resources/i18n/<locale>/main.json`
  — all 14 locales, otherwise those locales show the raw key.

## Traps worth knowing before you touch anything

1. **mpv option names are matched byte for byte.** `user-agent` works,
   `user_agent` is silently dropped (`m_config_get_co_raw` → `bstrcmp`). This bit us
   once in `client.cpp` and cost a debugging session.
2. **The app sends exactly one User-Agent.** `HTTP::default_user_agent()` feeds both
   libcurl and mpv. Cloud-drive front-ends sign the playback URL against the UA of
   the request that asked for it, so a second, independent UA is a `403` from the
   CDN — see `docs/playback.md`.
3. **Never hand-edit `scripts/patches/borealis-fixes.patch`.** Hunk offsets break
   silently. Regenerate it:
   ```sh
   cd library/borealis && git apply ../../scripts/patches/borealis-fixes.patch
   # edit the source, then:
   git diff > ../../scripts/patches/borealis-fixes.patch && git checkout -- .
   ```
   Verify with `git apply --check` before committing.
4. **Pure logic goes in a header under `app/include/`** so `tests/run.sh` can compile
   it without borealis or mpv. `utils/mpv_conf.hpp`, `api/jellyfin/playback_info.hpp`
   and `api/media/types.hpp` are examples.
5. **A new setting touches four places**: the `AppConfig::Item` enum + `settingMap`
   (`app/include/utils/config.hpp`, `app/src/utils/config.cpp`), the load in
   `AppConfig::init()`, the cell in `resources/xml/tabs/settings.xml` with its
   `BRLS_BIND` + `init()` in `tab/setting_tab.cpp`, and the i18n key.
6. **`mpv.conf` is only read for `user-agent`**, once, at startup. Any other option
   there is honoured by mpv itself, but the app cannot see it.

## Current state

Version 0.1.0, first SMCA release. Playback verified on hardware (Emby + a
cloud-drive 302 front-end), build verified by CI. `docs/backlog.md` lists what is
knowingly missing — read it before starting anything ambitious.
