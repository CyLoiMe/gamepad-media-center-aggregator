<!--
  用途：开 PR 时整段贴进描述框。英文，中性语气，无自述。
  开 PR 前把这个注释块和文件本身删掉即可（或保留文件、只在描述里用正文）。
  记得把末尾的 Fixes #N 换成真实 issue 号。
-->

## What

Two fixes for Emby/Jellyfin libraries whose media lives outside the server (`.strm` pointing at a cloud drive, `IsRemote` in the API):

1. play remote sources through the URL the server hands out in `PlaybackInfo`, instead of the URL we assemble ourselves;
2. send a single User-Agent from the app and from the player.

## Why

**The URL.** `resolvePlayback()` derived `/Videos/{id}/stream?static=true&mediaSourceId=…` from the MediaSource id. Stock Emby/Jellyfin accept it, but cloud-drive front-ends (TgtoDrive and similar 302 proxies) refuse it with `403` and answer `playback refused: request is missing the Emby User ID`: they need a client that first established a user context. Real Emby clients — Yamby, the web app, Emby Theater — `POST /Items/{id}/PlaybackInfo?UserId=…` and play `MediaSources[].DirectStreamUrl`, which those front-ends rewrite to the CDN's signed link.

**The UA.** `player_user_agent` only reached mpv: the HTTP client kept sending `GMCA/<version> (<platform>)`. Those front-ends sign the CDN link against the User-Agent of the request that asked for it (their resolution cache is keyed on `(strm, user-agent)`), so a player UA that differs is a guaranteed `403` from the CDN — a browser UA set in the settings made it strictly worse than the default.

## How

- `api/media/types.hpp`: `Media::remote`, decoded from `IsRemote` in `api/jellyfin/types.hpp`.
- `api/jellyfin/playback_info.hpp` (new): `directStreamUrl()` selects the `MediaSources` entry matching the playing version and returns its `DirectStreamUrl` when it is usable — absolute `http(s)`, and not a loopback/private/`.local` host unless it is the server's own (that is the internal address a server behind a reverse proxy reports). The token is appended when the URL is the server's own and lacks `api_key`.
- `api/jellyfin/backend.cpp`: `resolvePlayback()` calls it **only for remote sources**, and falls back to the URL it always built on any failure — the extra request can never break playback, and every other item keeps the exact previous path.
- `api/http.hpp` / `http.cpp`: `HTTP::default_user_agent()` becomes the single source of truth, with `set_default_user_agent()` and `builtin_user_agent()`.
- `utils/config.cpp`: resolves that one value from `player_user_agent`, else a `user-agent=` line in `mpv.conf`, else the built-in UA. `utils/mpv_conf.hpp` gained `lineValue()`/`value()` for that.
- `view/mpv_core.cpp`: passes exactly `HTTP::default_user_agent()` to mpv; the player-side copy of the setting is gone.
- `tab/setting_tab.cpp` + 14 locales: the settings cell updates that value and restarts an existing player; an empty value restores the built-in UA.
- `client/client.cpp`: the per-remote override used `user_agent=`, which mpv drops (option names are matched with `bstrcmp`); `user-agent=` now, quoted and escaped.

## Testing

- Switch, Emby + TgtoDrive + 115 CDN: `.strm` items on mp4 and mkv play; the front-end logs `PlaybackInfo … direct=1` and the player opens the CDN URL directly.
- `tests/test_jellyfin_playback.cpp`: the real response shape, source selection, private-host / relative / malformed rejections, token appending.
- `tests/test_mpv_conf.cpp`: value extraction from `mpv.conf` (comments, `--` prefix, blanks, quotes, byte-exact names, first occurrence wins).
- Normal (non-remote) playback on the same setup is unchanged; the CI builds all platforms.

## Not in this PR

- The transcode/HLS branch and `downloadUrl()` still hand out the assembled URL.
- A server that does not report `IsRemote` keeps the old path; widening the trigger to "always ask PlaybackInfo on direct play" is a one-line change if a deployment needs it.

Fixes #N
