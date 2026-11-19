# Architecture

One process, one window, one mpv instance. Everything is a borealis view on top of
two singletons.

## Layers

```
activity/     full-screen destinations pushed on the borealis activity stack
tab/          the five sidebar sections (home, libraries, playlists, remote, settings)
view/         reusable widgets + the player (mpv_core.cpp is the libmpv binding)
api/          media::Backend implementations: plex, jellyfin (also Emby), stremio
client/       file-system style clients: local, WebDAV, HTTP (Apache index), avio
utils/        config, downloads, offline catalog, images, misc helpers
```

`app/include` mirrors `app/src` one-to-one. `resources/` holds the XML layouts
(`resources/xml`), translations, fonts, SVG icons and the romfs payload that gets
packed into the NRO.

## The two singletons

| Singleton | Owns | Created |
|---|---|---|
| `AppConfig` (`utils/config.hpp`) | servers, users, settings (`config.json`), the active `media::Backend`, remotes and pins for the file browser | first `instance()` call, initialised in `AppConfig::init()` from `main()` |
| `MPVCore` (`view/mpv_core.hpp`) | the `mpv_handle`, the render context, playback state and events | lazily, at the first playback (`MPVCore::created()` tells you whether it exists) |

`AppConfig::init()` also pushes the persisted settings into the static knobs the
rest of the app reads (`MPVCore::HARDWARE_DEC`, `MPVCore::AUDIO_CHANNELS`,
`HTTP::set_default_user_agent()`, …). That is the only place settings become live
state.

## The backend interface

`media::Backend` (`api/backend.hpp`) is the contract between the UI and a server:
navigation verbs (sections, hubs, grids, detail, children, search…), item actions
(mark watched), playback (`resolvePlayback`), URL helpers (images, downloads, auth
headers) and the personal list (watchlist/favorites).

- Every backend produces the neutral `media::*` models in `api/media/types.hpp`
  (Plex, Jellyfin/Emby and Stremio all map onto them). The UI never sees a provider
  URL or a provider field name.
- `Capabilities` (`caps()`), returned per backend, pilots the UI: which tabs,
  filters, buttons and menus exist for the active server.
- Async convention: a verb runs on `brls::async`, then calls `then` back on the UI
  thread via `brls::sync`; failures go to `error`. `resolvePlayback` is the
  exception — it is synchronous and runs inside the caller's async task.

Backends are selected in `AppConfig::backendTypeFromString()` and built lazily by
`AppConfig::backend()`; `multi-backend.md` in this directory is the upstream design
note (French).

## Connections, profiles and tokens

A **connection** is one `AppUser` (a profile) bound to one `AppServer` through
`AppUser::server_id`. `config.json` stores the two lists; `ConnectionSwitcher`
(`view/connection_switcher.cpp`) renders one tile per `AppUser`.

| Field | Meaning |
|---|---|
| `AppServer::id` | Plex `clientIdentifier` / Jellyfin-Emby server Id / Stremio userId |
| `AppServer::type` | `plex` \| `jellyfin` \| `emby` \| `stremio` |
| `AppServer::access_token` | server credential — **only authoritative for Plex** |
| `AppServer::urls` | connection candidates, `front()` = last reachable |
| `AppUser::id` | Plex uuid (or `uuid@clientIdentifier`) / Jellyfin user Id / Stremio userId |
| `AppUser::access_token` | Plex: plex.tv token. Jellyfin/Emby: the account's AccessToken. Stremio: the authKey |

**The token a request must carry is not always the server's.** Jellyfin and Emby
issue one `AccessToken` per ACCOUNT, and two accounts on one server share a single
`AppServer` entry — its slot only holds whichever account authenticated last. That
is why the resolution below exists and why nothing may read `server_token`
directly:

```
backend request ──► AppConfig::getToken()                     (utils/config.cpp)
                      ├─ AppConfig::activeServerType()        type of the active AppUser's server
                      └─ connection::activeToken(type,        (utils/connection_token.hpp)
                              serverToken, userToken)
   Plex / Stremio  ─► server token
   Jellyfin / Emby ─► AppUser::access_token of the ACTIVE profile
```

**Switching** (`connectWithUser` in `view/connection_switcher.cpp`): block input +
loading screen → async: copy the `AppServer`, Plex-only token refresh from plex.tv,
`AppConfig::reachableUrl()` probe → sync: `addServer()` + `addUser()` (which
re-seats the active profile, `resetBackend()`, `applyTheme()`), clear the offline
flag, `Application::clear()` + `pushActivity(new MainActivity())`,
`MediaCollection::clearPref()`. The rebuild is what re-fetches everything: there is
no "connection changed" event, so anything cached outside the activity tree survives
a switch — `docs/backlog.md` §Known sharp edges lists what that is today.

**Startup** (`main.cpp`): `AppConfig::init()` loads `config.json`, then
`checkLogin()` re-probes the remembered URL of the persisted `user_id`. Failure with
a remembered server enters the offline shell (`utils/network_state.hpp`); a fresh
install goes to `ServerList`.

**Deleting** a connection: `AppConfig::removeUser()` drops the active state
(`user_id`/`server_url`/`server_token`/backend and the profile iterator) when the
deleted one was in use; the switcher then goes back to `ServerList`.

**Two addresses (Jellyfin/Emby).** `AppServer::local_url` is an optional home
address next to the remote candidates in `urls`. `AppConfig::reachableUrl()` builds
the candidate list from the `wan_policy` setting (Settings → Network) and reports
which one answered (`lan_mode`, runtime only; `server_url` remains the single active
address every backend reads through `getUrl()`):

| `wan_policy` | behaviour |
|---|---|
| `lan_first` (default) | LAN strictly preferred: the remote address only wins once the LAN probe has FAILED, so a faster tunnel answer never preempts a reachable home address. |
| `lan_only` | only the LAN address is probed; a home-only console can never spend tunnel quota. Roaming needs the policy changed — Settings is reachable from the logged-out root (Y). |
| `race` | first answer wins (opt-in): fastest, but at home the tunnel can win the race and burn quota. |

A LAN candidate fails fast (2 s) while a remote one keeps the 5 s budget, so a dead
home address does not hold up the fallback. `NetworkBadge`
(`view/network_badge.cpp`, top right) shows LAN/WAN and opens its dialog on **ZR**
(R is already next-tab); it can re-probe or force the other address.

`AppUser::wan_visible_libraries` is the per-profile WAN allow-list this mode exists
to enforce. `AppConfig::allowedLibraries()` snapshots it on the UI thread (nullopt =
not configured = nothing hidden) and the Jellyfin/Emby backend then queries per
VISIBLE library — sidebar, home rows, Continue Watching, Recently Added, search —
so a hidden library costs neither a request nor its artwork. The rule itself is
`utils/wan_filter.hpp`; `docs/backlog.md` lists the surfaces still to cover.

The lists are not the last word: an item can still reach the player from a
cross-library surface (favorites, playlist, deep link). `player_view.cpp` therefore
guards the handoff to mpv — it resolves the item's library (`librarySectionID`, else
`sectionOfSync` → Ancestors) and refuses playback from a hidden library, or from an
unresolved one unless the resolved URL leaves the server host. `utils/wan_guard.hpp`
holds that rule, and it is the only place that decides it.

## Where things live

| Area | Entry point |
|---|---|
| process start, args, forwarder | `app/src/main.cpp` |
| activity stack, sidebar, per-server layout | `app/src/activity/main_activity.cpp` |
| connection tiles, switching, deleting | `app/src/view/connection_switcher.cpp` |
| persisted state, active connection, tokens | `app/include/utils/config.hpp`, `app/src/utils/config.cpp` |
| per-backend token rule (pure, tested) | `app/include/utils/connection_token.hpp` |
| offline flag / offline shell | `app/include/utils/network_state.hpp`, `app/src/utils/offline_ui.cpp` |
| provider adapters | `app/src/api/{plex,jellyfin,stremio}/` |
| playback policy | `app/src/activity/player_view.cpp` → `api/backend.hpp` `resolvePlayback` |
| WAN filter + playback guard rules (pure, tested) | `app/include/utils/wan_filter.hpp`, `app/include/utils/wan_guard.hpp` |
| WAN library visibility (X column) | `app/src/view/library_manager.cpp` |
| LAN/WAN badge + status dialog | `app/src/view/network_badge.cpp` |
| downloads / offline catalog | `app/src/utils/download.cpp`, `app/src/utils/offline_library.cpp` |
| artwork loading + disk cache | `app/src/utils/image.cpp`, `app/include/utils/image_cache.hpp` |
| app icons / brand art | `brand/logos/logo.jpg` → `scripts/make-icons.py` → `resources/icon/` (`brand/README.md`) |

## A screen, end to end

```
main.cpp                     brls::Application::pushActivity(new MainActivity())
activity/main_activity.cpp   sidebar + tabs, per-server layout from config
tab/home_tab.cpp             hub rows  ── backend.getHomeHubs(then, error)
view/recycling_grid.cpp      cards     ── media::Item → VideoCardCell
tab/media_movie.cpp          detail    ── backend.getItemDetail(id, full=true)
activity/player_view.cpp     playback  ── backend.resolvePlayback(item, version, opts)
view/mpv_core.cpp            mpv loadfile + render into the borealis frame
```

`activity/player_view.cpp` is where playback policy lives: which media version,
transcode or direct play, the fallback to direct play when a transcode fails,
progress reporting, the WAN guard that can refuse the playback outright, and the
error dialog.

## Where to add things

| Task | Start at |
|---|---|
| a new screen | `app/include/activity/` + `app/src/activity/`, XML in `resources/xml/activity` |
| a new settings row | `docs/conventions.md` §settings (four places) |
| a new backend | implement `media::Backend`, register in `AppConfig::backendTypeFromString` + `BackendType`, add caps |
| a new playback option | `PlaybackOptions` (`api/backend.hpp`) → backend `resolvePlayback` → `MPVCore` |
| a new string | `resources/i18n/*/main.json` (all locales) |
| anything about the active server/profile or its token | `AppConfig::getToken()` / `activeServerType()`; rule in `utils/connection_token.hpp` |
| a connection tile, switching or deleting | `view/connection_switcher.cpp`, `AppConfig::addUser`/`removeUser` |
| app icons / brand art | `brand/README.md` + `scripts/make-icons.py` ({Pillow}) |

## Concurrency and lifetime

- UI work happens on the borealis main thread; `brls::async` is **one** worker
  thread (FIFO), so long blocking calls queued behind each other delay playback.
- Views are deleted by the activity stack; async callbacks must use the
  `ASYNC_RETAIN`/`ASYNC_TOKEN`/`ASYNC_RELEASE` dance (`utils/event.hpp`) or capture
  only plain values.
- `MPVCore` is destroyed on the application exit event, not by the views.

## Platform

Switch only. `app/src/main.cpp` has the Switch-specific startup paths (hbmenu vs
application mode, forwarder detection); everything else is portable code that just
happens to have no other build target.
