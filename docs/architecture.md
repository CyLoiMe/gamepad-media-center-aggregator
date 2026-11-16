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
