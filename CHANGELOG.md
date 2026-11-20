# Changelog

## [Unreleased]

### Networking

- **Two addresses per Jellyfin/Emby connection.** An optional home address
  (`AppServer::local_url`) is probed before the remote one; Settings → Network
  picks the policy (`lan_first` / `lan_only` / `race`). The top-right badge shows
  LAN or WAN and its dialog (**ZR**) can re-probe or force the other address.
- **Home-address probe budget.** Settings → Network now sets how long the home
  address gets before the tunnel is used (1.5 s – 5 s, default 2 s), so a slow
  home Wi-Fi is not read as "away" and does not fall through to the metered
  address. The sign-in form probes with the same value.
- **WAN library allow-list.** Settings → Libraries gained an **X** column that
  decides, per library, whether it exists on the metered address; the Jellyfin/Emby
  backend then queries only the visible libraries. Playback is guarded too: an item
  from a hidden library is refused before it reaches mpv.
- **Downloads obey the allow-list.** Queueing an item whose library is hidden on the
  metered address is refused before anything is written, and the check runs again
  when a transfer starts — the address and the allow-list can change while an item
  waits, and the queue even outlives a restart. A download spends the whole file
  rather than the few minutes a playback costs, so it asks the player's rule
  (`utils/wan_guard.hpp`), not a second one of its own: a library allowed on WAN
  downloads as before, and a season refused episode by episode says so once.
- **Music obeys the allow-list too.** The audio player had no guard at all: it
  handed mpv a resolved track URL whatever the address was, so an album from a
  hidden library streamed on the metered one. It now asks the same rule before
  `setUrl`, and a refusal drops the queue instead of leaving it loaded — that queue
  would advance at the end of the track and be refused again, one track at a time,
  with silence in between.
- **Favorites and playlists obey the allow-list.** Neither endpoint takes a library
  scope, so their pages are filtered as they arrive: a movie or a show names its
  library in `ParentId`, an episode is resolved through its show once per page, and
  a page that is entirely hidden is skipped instead of ending the list early.
- **Edit a connection in place** (Y on a connection tile): addresses and
  credentials without delete-and-re-add, so the allow-list and the sidebar layout
  survive. An unchanged username with a blank password only rewrites the addresses
  and keeps the stored token.

### Fixed

- A library's sort and filter no longer leak between servers, or into the next
  library. The stored preference was keyed by the section id alone and Plex numbers
  sections 1, 2, 3…, so two servers shared one entry; the key now carries the
  connection, existing entries are adopted once, switching no longer wipes
  everyone's preferences, and a library with no stored preference starts from the
  defaults instead of the selection the last library left behind.
- A cached poster can no longer answer for another server. Artwork is keyed by the
  connection that fetched it, since a relative path is that server's own id; offline
  the cache still accepts a copy from another connection rather than showing a blank
  poster, because nothing can be re-fetched there.
- A queued download no longer resumes against whichever connection is active. Each
  item is stamped with the server and profile that queued it, and a transfer only
  starts under that connection: asking another server for a part key it does not
  have is a 404 at best and, where part ids collide, a different file. The row reads
  "Waiting for its server" until the connection comes back, and switching to it
  resumes the queue.
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
