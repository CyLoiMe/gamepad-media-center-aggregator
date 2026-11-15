# Remote (`.strm`) playback behind a 302 proxy — what was missing

Status: working (verified on Switch against Emby + a TgtoDrive-style 302 front-end).
Audience: maintainers. Every domain, token and signature below is redacted.

## Symptom

Library browsing worked, playback of any cloud-drive-backed item failed instantly:

```
[  10.748][d][cplayer] Run command: loadfile, url="https://<emby-or-proxy>/Videos/1223/stream?static=true&mediaSourceId=…&api_key=…"
[  10.754][v][ffmpeg] Opening https://<emby-or-proxy>/Videos/1223/stream?static=true&…
[  11.046][w][ffmpeg] https: HTTP error 403 Forbidden
[  11.051][v][cplayer] finished playback, loading failed (reason 4)
```

No `302` ever happened: the very first request was rejected. Setting a browser
`User-Agent` changed nothing.

The front-end in front of Emby (TgtoDrive, a Python/WSGI service that turns cloud
drive shares into `.strm` files and rewrites playback to the drive CDN) answered:

```json
{"error": "playback refused: request is missing the Emby User ID", "stage": "stream", "success": false}
```

and on its own log:

```
WARNING  拒绝缺少 Emby UserId 的播放请求: method=GET, path=/Videos/1223/stream
         (refused: playback request without an Emby UserId)
```

## Root cause 1 — the client builds the play URL itself

`JellyfinBackend::resolvePlayback()` never asked the server how to play an item: it
assembled `/Videos/{id}/stream?static=true&mediaSourceId=…` from the MediaSource id
(`apiVideoStream` in `app/include/api/jellyfin/types.hpp`) and handed that to mpv.
That URL is fine on stock Emby/Jellyfin, which is why nobody noticed, but it is the
one URL such front-ends refuse — they need the request to come from a client that
first established a user context.

Real Emby clients (Yamby, Emby web, …) do it the other way round:

1. `POST /Items/{id}/PlaybackInfo?UserId=<user id>` — establishes the user/device;
2. the response's `MediaSources[i].DirectStreamUrl` is what they play.

For a `.strm` on a cloud drive, that field is where the front-end puts the URL it
wants the client to use. In the reporting setup:

```json
{
  "MediaSources": [{
    "Id": "mediasource_1223",
    "IsRemote": true,
    "Path": "http://192.168.x.x:12366/play115/<pickcode>/…/<file>.mp4",
    "AddApiKeyToDirectStreamUrl": false,
    "DirectStreamUrl": "https://<cdn-host>/<hash>/<file>.mp4?t=…&u=…&k=…&v=1"
  }]
}
```

`DirectStreamUrl` is the cloud CDN's signed link, i.e. the `302` target — served
directly, without an extra hop. Anything else is a `403`.

## Root cause 2 — the app had two User-Agents

While investigating, a second bug surfaced: `player_user_agent` (added in this
series) only reached mpv, because `mpv_set_option_string(mpv, "user-agent", …)` was
fed from a player-side copy. The app's own HTTP client kept sending
`GMCA/<version> (<platform>)`.

That matters here because such front-ends sign the CDN link **against the
User-Agent of the request that asked for it**:

```
INFO /play115[download_url] 即将调用 client.download_url: pickcode=… kwargs_keys=app,extensions,request,user_agent
INFO /play115[strm+ua] 缓存写入: canonical_strm=… ua=GMCA/1.4.0 (NX)
```

— the `user_agent` is forwarded to the drive client, and the resolution cache key is
`(strm, user_agent)`. So the UA used to *fetch the link* and the UA used to *play it*
must be byte-identical; a browser UA only in mpv was strictly worse than none.

## Changes

### 1. Play remote sources through `PlaybackInfo` (`feat(jellyfin)`)

- `app/include/api/media/types.hpp` — `Media::remote`, decoded from the Jellyfin/Emby
  `IsRemote` flag in `api/jellyfin/types.hpp` (`parseMediaSource`).
- `app/include/api/jellyfin/playback_info.hpp` (new, pure logic, unit-tested) —
  `directStreamUrl(response, sourceId, serverUrl, token)` picks the `MediaSources`
  entry matching the chosen source and returns its `DirectStreamUrl`, or `""` when
  there is nothing usable:
  - relative or non-`http(s)` URLs are ignored;
  - loopback / RFC1918 / `169.254` / `.local` hosts are ignored **unless** they are
    the server's own host. That is the address a server behind a reverse proxy
    reports for itself; it is unreachable from a console on another network, and
    rejecting it keeps the previous behaviour for those setups;
  - when the URL is the server's own host and carries no `api_key`, the token is
    appended (stock Emby usually adds it itself, the front-ends above do not).
- `app/src/api/jellyfin/backend.cpp` — `resolvePlayback()` asks `PlaybackInfo` **only
  when the selected source is remote**, then falls back to the URL it always built
  if that fails for any reason (network error, no field, unusable URL). Normal items
  keep the exact previous code path, and the extra request can never break playback.
- `tests/test_jellyfin_playback.cpp` — the real response shape above, source
  selection, the private-host/relative/malformed rejections, token appending.

Transcode/HLS and the download URLs still use the previously built URL; they are
untouched by this change.

### 2. One User-Agent for the whole app (`fix(http)`)

- `HTTP::default_user_agent()` is now the single source of truth;
  `HTTP::set_default_user_agent()` sets it and `HTTP::builtin_user_agent()` exposes
  the built-in `<package>/<version> (<platform>)` value an empty setting falls back
  to (`app/include/api/http.hpp`, `app/src/api/http.cpp`).
- `AppConfig::init()` resolves it once: `player_user_agent` (config.json / settings)
  → a `user-agent=` line in `{configDir}/mpv.conf` → the built-in one. `utils/mpv_conf.hpp`
  gained `lineValue()`/`value()` (it previously only answered "does the file set
  this option", which cannot feed a value); `tests/test_mpv_conf.cpp` covers it.
- `app/src/view/mpv_core.cpp` now always passes exactly
  `HTTP::default_user_agent()` to mpv; the player-side copy of the setting is gone,
  so the two sides cannot drift.
- Settings → Network → "Custom User-Agent" (`setting_tab.cpp`, 14 locales) writes the
  setting, updates the HTTP client and restarts an existing player. Clearing it
  restores the built-in UA.
- `app/src/client/client.cpp` — the per-remote override used `user_agent=`, which mpv
  silently drops: option names are matched byte for byte
  (`m_config_get_co_raw()` → `bstrcmp`), so only the curl half of the override ever
  worked. Now `user-agent=` (quoted, escaped).

## Verification

- Front-end log after the change: `PlaybackInfo 处理成功: path=/Items/1223/PlaybackInfo,
  direct=1` with `ua=GMCA/1.4.0 (NX)`, then the client opens the CDN URL and playback
  starts. Two items (mp4 + mkv) confirmed.
- `tests/test_mpv_conf.cpp` and `tests/test_jellyfin_playback.cpp` are standalone
  (`tests/run.sh`, no CMake wiring).
- CI (`build`, `platform=nx`) builds the NRO with these changes.

## Notes / follow-ups

- The `IsRemote` gate means a server that does not report the flag keeps the old
  path. If a deployment needs it anyway, widening the trigger to "always ask
  PlaybackInfo on direct play" is a one-line change — at the cost of one extra
  request per playback.
- Worth doing next: the same treatment for the transcode/HLS branch and for
  `downloadUrl()`, which still hand out the assembled URL.
- `mpv.conf`'s `user-agent=` is read once at startup (it is one value for the whole
  process); changing the file needs an app restart, the settings cell does not.
