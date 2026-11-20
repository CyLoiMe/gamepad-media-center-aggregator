# Changelog

## [Unreleased]

### Networking

- **Two addresses per Jellyfin/Emby connection.** An optional home address
  (`AppServer::local_url`) is probed before the remote one; Settings → Network
  picks the policy (`lan_first` / `lan_only` / `race`). The top-right badge shows
  LAN or WAN and its dialog (**ZR**) can re-probe or force the other address.
- **WAN library allow-list.** Settings → Libraries gained an **X** column that
  decides, per library, whether it exists on the metered address; the Jellyfin/Emby
  backend then queries only the visible libraries. Playback is guarded too: an item
  from a hidden library is refused before it reaches mpv.
- **Favorites and playlists obey the allow-list.** Neither endpoint takes a library
  scope, so their pages are filtered as they arrive: a movie or a show names its
  library in `ParentId`, an episode is resolved through its show once per page, and
  a page that is entirely hidden is skipped instead of ending the list early.
- **Edit a connection in place** (Y on a connection tile): addresses and
  credentials without delete-and-re-add, so the allow-list and the sidebar layout
  survive. An unchanged username with a blank password only rewrites the addresses
  and keeps the stored token.

### Fixed

- Adding a second account on a server that already exists no longer drops the home
  address typed on that form: the address lives on the server entry, so it is
  adopted there. The sign-in form now says the addresses belong to the server, which
  is why two accounts on one server always show the same pair.
- Installing the HOME tile (Settings → Home menu icon) now reports a failure
  instead of showing "done": the install result was ignored and the title ID read
  afterwards was undefined, so a failed install launched the previously installed
  tile and left its old icon on screen.
- Streams from a cloud-drive front-end (TgtoDrive and similar) behind a 302 play
  through the server's `PlaybackInfo` URL instead of an assembled one.
- The app and mpv send the same User-Agent; the CDN signature binds the two.

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
