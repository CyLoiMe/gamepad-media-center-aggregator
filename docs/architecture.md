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
progress reporting and the error dialog.

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
