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

## WAN bandwidth protection

A connection may carry a home address (`AppServer::local_url`) and a remote one
behind a metered tunnel. The dual-address probe, the LAN/WAN badge and the ZR
status dialog exist (`utils/dual_url.hpp`, `view/network_badge.cpp`); what remains
is the part that actually protects the quota.

- [x] **Enforce `AppUser::wan_visible_libraries`.** The sidebar (`listSections`),
      the home rows (`Next Up` and `Latest` per library), `Continue Watching`,
      `Recently Added` and `search` now query per VISIBLE library, so a hidden one
      contributes neither a row nor the artwork its row would have fetched.
      Filtering responses instead would have broken `TotalRecordCount` and the page
      sizes, and `ParentId` is the season for an episode, not its library.
- [ ] The personal list (favorites) and playlists are still cross-library: they can
      still surface an item from a hidden library. Both need an item's owning
      library, which is the `librarySectionID` item below.
- [x] **Playback guard.** `player_view.cpp` snapshots the allow-list, resolves the
      item's library (its `librarySectionID`, else `sectionOfSync` → Ancestors) and
      refuses to hand mpv a URL that would spend quota: a hidden library always
      blocks; an unresolved library blocks unless the resolved URL is an external
      CDN link. A library the user explicitly allowed on WAN stays playable even
      when its bytes come from the server. `utils/wan_guard.hpp` holds the rule,
      `tests/test_wan_guard.cpp` covers it.
- [x] **Library visibility matrix.** The sidebar manager (Settings → Libraries)
      already lists exactly the connection's libraries, so it carries the matrix as
      an **X** column instead of a second screen: X allows/hides the focused library
      on WAN and writes `AppUser::wan_visible_libraries`. Turning everything back on
      stores `nullopt`, so a library added to the server later starts visible.
- [x] **Edit an existing connection.** The pencil on a connection tile (**Y**, the
      tab switcher owns R) re-opens the sign-in form pre-filled and saves in place
      through `AppConfig::updateConnection` — never delete-and-re-add. A blank
      password with an unchanged username rewrites only the addresses and reuses
      the stored token, so a typo in the URL costs nothing else. A real re-login
      keeps what belongs to the PROFILE even when the server or user Id comes back
      different: the WAN allow-list (on the profile) and the sidebar layout (keyed
      by server id) are migrated, the stale server entry is dropped only when no
      other account still points at it, and the shell is rebuilt when the edited
      connection was the one being browsed. Rule: `utils/connection_edit.hpp`,
      covered by `tests/test_connection_edit.cpp`.
- [ ] `librarySectionID` is not populated by the Jellyfin/Emby mapper, so a listed
      item cannot say which library it belongs to. The playback guard resolves it on
      demand (`sectionOfSync` → Ancestors), which is exact but costs a request per
      guarded playback; filling it in the mapper would let favorites and playlists
      be filtered without that call. Prefer `Ancestors` — `ParentId` is the season
      for an episode, not the library.

## Product

- [ ] Drop the Plex and Stremio backends if they are not wanted: `media::Backend`
      stays, but the implementations, their `Capabilities`, the connection screens,
      the watchlist tab and a good part of the i18n go away. High churn, low
      urgency — do it only if maintaining them is a real cost.
- [ ] Verify the offline catalog for Jellyfin/Emby against a real server (inherited
      as "best-effort / unverified" from upstream).

## Reported on hardware, 2026-11-19 (to investigate)

Two symptoms reported after the first build that contains the dual-address work.
Neither is confirmed as a defect yet — the first checks are listed with each one.

- [ ] **Two Emby accounts end up sharing one LAN address.** Both entries originally
      carried only the remote address; after the LAN address was filled in on one,
      both show the same value.
      *Expected by design, needs confirming:* the addresses live on `AppServer`,
      which is keyed by the **server Id** — two accounts on the SAME server share
      one entry, and `updateConnection()` (`app/src/utils/config.cpp`, the "server
      entry" block) writes `urls`/`local_url` there. What is per PROFILE is the WAN
      allow-list (`AppUser::wan_visible_libraries`). Verify with
      `sdmc:/switch/SMCA/config.json`: if both users have the same `server_id`, one
      `AppServer` entry is correct and only the UI wording is misleading; if the
      ids differ, `updateConnection` matched the wrong entry and it is a real bug.
      Either way the form should say that the addresses belong to the server, not
      to the account.
- [x] **No WAN column in Settings → Libraries.** Cause found: the screen existed but
      its settings row was still labelled `侧边栏` / "Sidebar" (`setting/libraries/header`),
      from when the page only reordered the sidebar, so nobody looked for a library
      manager there. The label is now "Libraries & sidebar" in all 14 locales and the
      description names the **X** key. The screen is the same one that reorders the
      sidebar: its rows are the libraries, **A** reorders, **Y** hides a tab and **X**
      toggles WAN visibility.
      If the column is STILL missing, the checks remain: `LibraryManager::wanEnabled`
      needs `activeServerType()` to be `jellyfin`/`emby` **and** `getUserId()` non-empty
      (`app/src/view/library_manager.cpp:231`); the row list comes from
      `MainTabFrame::getReorderableEntries()`, so it is empty until the libraries are
      loaded; and the settings cell finds that frame by walking the view parents, so a
      Settings screen outside the tab frame makes the click a silent no-op
      (`tab/setting_tab.cpp:510`).
