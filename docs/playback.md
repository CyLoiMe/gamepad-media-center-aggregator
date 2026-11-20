# Playback

## The path of one play

```
PlayerView::playMedia()                       activity/player_view.cpp
  ├─ chooses the media version, builds media::PlaybackOptions (seek, codec cap,
  │  audio/subtitle stream, session id)
  └─ brls::async → backend.resolvePlayback(item, version, opts)
        └─ media::PlaybackSource { url, mpvExtra, isTranscode, transcodeSession }
  └─ brls::sync → MPVCore::setUrl(url, mpvExtra)
        └─ mpv "loadfile <url> replace 0 <mpvExtra>"
              └─ ffmpeg opens the stream; the video is rendered into the
                 borealis frame by mpv_render_context
```

`mpvExtra` is an mpv option list (`network-timeout=30,http-proxy="…",start=…`) —
that is how per-playback options travel; global options are set in
`MPVCore::init()` before `mpv_initialize()`.

Failure surfaces through `MPVCore::getError()`: the first `ffmpeg`/`stream` WARN
plus the first ERROR line since the load, both URL-redacted
(`utils/log_redact.hpp`), shown in the player's error dialog.

## Remote sources (`.strm` on a cloud drive)

A Jellyfin/Emby item can be `IsRemote`: the server stores a `.strm` whose target
lives on a cloud drive, and the playback URL may be served by a front-end that
rewrites it to a 302 (TgtoDrive and similar).

Stock Emby/Jellyfin accept `/Videos/{id}/stream?static=true&mediaSourceId=…`, which
is what this app used to build itself. Those front-ends **refuse** it (`403`,
"missing Emby UserId") because they expect the client to establish a user context
first, the way every real Emby client does:

1. `POST /Items/{id}/PlaybackInfo?UserId=<user id>`;
2. play `MediaSources[i].DirectStreamUrl` — for a remote source the front-end has
   rewritten it to the CDN's signed link.

`JellyfinBackend::resolvePlayback()` therefore asks PlaybackInfo **only when the
selected source is remote** (`Media::remote`, decoded from `IsRemote`), and keeps
the assembled URL for everything else and as a fallback on any failure. The
selection rules live in `api/jellyfin/playback_info.hpp` (pure, unit-tested):
relative or non-`http(s)` URLs are ignored, and so are loopback/private/`.local`
hosts — the address a server behind a reverse proxy reports for itself, which a
console on another network cannot reach — unless the host is the server's own, in
which case the token is appended when missing.

The reasoning, the logs from both sides and the limits are written up in
`emby-remote-playback.md` (and its Chinese version), with the issue/PR drafts in
`upstream/`.

**A remote source is never transcoded.** The URL the client ends up with is the
CDN's copy of the original file: there is no encoder on that side, and asking the
server to transcode means pulling the whole file through the server — which is what
these setups exist to avoid, and usually cannot do at all. A cap is what routes
`resolvePlayback()` into the transcode branch, so `player_view.cpp` zeroes it and
skips the resolution-limit derivation (the Switch defaults to `RESOLUTION_LIMIT` at
1080p, which would otherwise push every 4K cloud item into a transcode) for
`media::remote`. The OSD quality menu is hidden for the same reason — every tier of
it is a transcode — and `toggleQuality()` answers with `main/player/quality_remote`
if the bound key is pressed anyway.

The one exception is `tryTranscodeFallback()`: it only runs after a direct play has
already failed, and a transcode that might be slow is still better than a guaranteed
error.

## User-Agent: one value, for the whole app

Cloud-drive front-ends **sign the CDN link against the User-Agent of the request
that asked for it** (their resolution cache is keyed on `(strm, user-agent)`), and
the CDN then compares it with the UA of the playback request. Two UAs = `403`.

Therefore:

- `HTTP::default_user_agent()` (`api/http.hpp`) is the single source of truth; the
  libcurl handles set it at construction and `MPVCore::init()` passes the same
  string to mpv.
- `AppConfig::init()` resolves it once, in this order: `player_user_agent`
  (`config.json`, Settings → Network) → a `user-agent=` line in
  `{configDir}/mpv.conf` → the built-in `SMCA/<version> (NX)`.
- Changing it in the settings writes `HTTP::set_default_user_agent()` immediately
  and restarts an existing player; an empty value restores the built-in UA.
- `mpv.conf` is read **only for this**, once, at startup: editing the file needs an
  app restart. Every other option in it is applied by mpv itself.

Per-remote overrides (`client/client.cpp`, WebDAV/HTTP browsing) still exist and are
passed as a loadfile option — spelled `user-agent=`, because mpv matches option
names byte for byte.
