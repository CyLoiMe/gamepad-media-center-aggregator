# Backlog

Known work, roughly in the order it should be picked up. Nothing here is a promise.

## Before the first public release

- [x] **Replace the icon.** SMCA's own artwork now lives in `brand/logos/logo.jpg`
      and `scripts/make-icons.py` derives the NRO icon, the HOME tile and the in-app
      logos from it (`brand/README.md`). The NACP name it lands in is already SMCA's.
- [ ] **Create the GitHub repository the updater reads.** `AppVersion::git_repo`
      (`app/include/utils/config.hpp`) points at
      `CyLoiMe/switch-media-center-aggregator`; the in-app update check 404s until
      that repository exists (or edit the one line).
- [ ] **Host the portlibs.** `DEPS_URL` in `.github/workflows/switch.yaml` and
      `scripts/build-switch.sh` still borrows GMCA's `ci-deps` release. Run the
      `switch-portlibs` workflow, publish the packages as a `ci-deps` prerelease
      here, repoint both places (see `releasing.md`).

## Cleanup carried over from upstream

- [ ] `app/src/utils/vita_install.cpp` + `vita_head_bin.h` are unreachable on
      Switch (no PS Vita build). Removing them also removes the corresponding
      GPL-derived artifact note from `NOTICE` — do both in one change, and keep the
      header-only `reached through the shared update path` comment truthful.
- [ ] `#ifdef __PSV__ / __PS4__ / ANDROID / _WIN32 / __APPLE__` blocks remain in
      shared sources (`mpv_core.cpp`, `version.cpp`, `config.cpp`, …). Harmless dead
      code today; prune when touching those files anyway.
- [ ] `docs/spec.md` and `docs/multi-backend.md` are upstream design docs in French.
      Useful as reference, but they describe a multi-platform, multi-store app.

## Playback

- [ ] The transcode/HLS branch and `downloadUrl()` still hand out the assembled
      `/Videos/{id}/stream` URL. Remote sources on a 302 front-end will fail there
      the same way direct play used to — apply the PlaybackInfo treatment
      (`docs/playback.md`) if it shows up.
- [ ] A server that does not report `IsRemote` keeps the old path. Widening the
      trigger to "always ask PlaybackInfo on direct play" is a one-line change, at
      the cost of one extra request per playback.
- [ ] `mpv.conf` is only consulted for `user-agent`. Full support (filters, cache,
      vo tuning) would need the app to stop forcing the options it currently sets.

## Known sharp edges

State that outlives a connection switch. There is no "connection changed" event: the
switch rebuilds `MainActivity` and everything inside it, but nothing outside.

- [ ] `MediaCollection::customPrefs` (`LIBRARY_SORT`) is keyed by the section
      `itemId` alone, not by server — two servers emitting the same section key share
      the stored sort/filter. `MediaCollection::clearPref()` runs only in the
      switcher's own switch path, not in the Plex/Jellyfin/Stremio add flows.
- [ ] `ImageCache` keys on the raw artwork path with no server id
      (`utils/image_cache.hpp`), so a cached poster can answer for another server.
- [ ] `DownloadManager` resolves the URL and auth headers from the *live* backend at
      transfer time (`utils/download.cpp`), so a queued item resumes against
      whichever connection is active.
- [ ] `AudioPlayer`'s queue is not cleared on a switch — only `player_view` and
      `remote_view` call `release()`.

## Product

- [ ] Drop the Plex and Stremio backends if they are not wanted: `media::Backend`
      stays, but the implementations, their `Capabilities`, the connection screens,
      the watchlist tab and a good part of the i18n go away. High churn, low
      urgency — do it only if maintaining them is a real cost.
- [ ] Verify the offline catalog for Jellyfin/Emby against a real server (inherited
      as "best-effort / unverified" from upstream).
