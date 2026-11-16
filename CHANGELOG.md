# Changelog

## [0.1.0] - 2026-11-16

First SMCA release. SMCA (Switch Media Center Aggregator) is a Nintendo Switch
client forked from [GMCA](https://github.com/thcolin/gamepad-media-center-aggregator)
by thcolin (Apache-2.0), rebuilt as a Switch-only project.

### Servers

- Plex, Jellyfin, Emby and Stremio behind one backend interface, switchable from
  the connection screen; per-backend accent colour and tabs.

### Playback

- libmpv with hardware decoding (deko3d), direct play with automatic transcode,
  resume positions, audio/subtitle track selection, speed and aspect controls.
- Remote (`.strm`) sources on a cloud drive behind a 302 front-end (TgtoDrive and
  similar) now play through the `DirectStreamUrl` the server returns in
  `PlaybackInfo`, instead of a URL the client assembles itself.
- One User-Agent for the whole app — the HTTP client and the player send the same
  string, which is what such front-ends sign the playback URL against. Settable in
  Settings → Network or with `user-agent=` in `mpv.conf`.

### Library and files

- Offline downloads with a local catalog, SD card / WebDAV / HTTP / USB (UMS)
  browsing, subtitles (embedded and sidecar), 14 UI languages.

### Removed

- Every non-Switch platform of the upstream project (PS Vita, PS4, Windows, macOS,
  Linux, Android), their packaging and their CI jobs: the build now refuses to
  configure anything but `PLATFORM_SWITCH`.
