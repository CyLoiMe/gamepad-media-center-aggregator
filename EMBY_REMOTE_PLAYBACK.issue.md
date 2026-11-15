<!--
  用途：向 thcolin/gamepad-media-center-aggregator 提 Issue 时，标题和正文直接复制下面两段。
  正文用英文（仓库工作语言）；细节放在分支里的 EMBY_REMOTE_PLAYBACK.md，正文里点名引用即可。
  开 PR 前可以把这个草稿文件删掉，不必提交。
-->

## 标题

Emby/Jellyfin: `.strm` playback behind a 302 proxy (TgtoDrive) fails with 403

## 正文

Playing any `.strm` item whose library is served through a 302 front-end fails instantly with `mpv -13 / ffmpeg: https: HTTP error 403 Forbidden`, while the same item plays in Yamby and in the Emby web client; browsing works and no redirect is ever issued.

Cause: `resolvePlayback()` builds `/Videos/{id}/stream?static=true&…` itself. Such front-ends refuse that URL (they answer "missing Emby UserId") and expect clients to ask `/Items/{id}/PlaybackInfo` first and play `MediaSources[].DirectStreamUrl`, which they rewrite to the cloud CDN's signed link.

Second cause, found while debugging: the app sent two different User-Agents (mpv vs its own HTTP client). These front-ends sign the CDN link against the UA of the request that asked for it, so the two must be identical — and `player_user_agent` only reached the player.

Fix is ready on my fork, branch `ua-302`: ask `PlaybackInfo` for remote (`IsRemote`) sources and play its `DirectStreamUrl` (falling back to the current URL on any failure), plus one User-Agent for the whole app. Root cause, log evidence and limits are written up in `EMBY_REMOTE_PLAYBACK.md` on that branch.

Verified on Switch against Emby + TgtoDrive + 115 CDN (mp4 and mkv), with unit tests for the PlaybackInfo selection and the mpv.conf User-Agent parsing. Would you take this as a PR against `dev`?
