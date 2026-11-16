# 302 反代后面的远端（`.strm`）播放：缺了什么

状态：已修复并在真机验证（Switch + Emby + TgtoDrive 风格的 302 前置）。
读者：想了解这次改动的人。文中所有域名、token、签名均已打码。

## 现象

库浏览正常，但任何由网盘承载的条目一播就失败：

```
[  10.748][d][cplayer] Run command: loadfile, url="https://<emby-or-proxy>/Videos/1223/stream?static=true&mediaSourceId=…&api_key=…"
[  10.754][v][ffmpeg] Opening https://<emby-or-proxy>/Videos/1223/stream?static=true&…
[  11.046][w][ffmpeg] https: HTTP error 403 Forbidden
[  11.051][v][cplayer] finished playback, loading failed (reason 4)
```

**整个过程没有发生过 302**：第一个请求就被拒了。把 User-Agent 换成浏览器 UA 也毫无变化。

Emby 前面那层前置（TgtoDrive，一个把网盘分享变成 `.strm`、并把播放改写为网盘 CDN 直链的 Python/WSGI 服务）给出的拒绝原因是：

```json
{"error": "播放被拒绝：请求中缺少 Emby User ID，无法确认播放用户。", "stage": "stream", "success": false}
```

它自己的日志：

```
WARNING  拒绝缺少 Emby UserId 的播放请求: method=GET, path=/Videos/1223/stream
```

## 根因一：客户端自己拼了播放地址

`JellyfinBackend::resolvePlayback()` 从来没有问过服务器"这个条目该怎么播"，而是拿 MediaSource id 拼出
`/Videos/{id}/stream?static=true&mediaSourceId=…`（`app/include/api/jellyfin/types.hpp` 里的 `apiVideoStream`）直接交给 mpv。

这个 URL 在原生 Emby/Jellyfin 上是对的，所以一直没人发现；但恰恰是这类前置**只认**"先建立过用户上下文的客户端"发出的请求。

真正的 Emby 客户端（Yamby、Emby 网页版……）走的是另一条路：

1. `POST /Items/{id}/PlaybackInfo?UserId=<用户 id>` —— 建立用户/设备上下文；
2. 播它返回的 `MediaSources[i].DirectStreamUrl`。

对挂在网盘上的 `.strm`，这个字段就是前置希望客户端使用的地址。实际抓到的响应：

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

`DirectStreamUrl` 就是网盘 CDN 的签名直链，也就是 302 的目标——前置把它直接给了客户端，少一跳。用别的地址一律 403。

## 根因二：App 里存在两个 User-Agent

排查过程中暴露出第二个 bug：本系列新增的 `player_user_agent` **只作用于 mpv**（`mpv_set_option_string(mpv, "user-agent", …)` 读的是播放器侧的一份副本），App 自己的 HTTP 客户端始终发 `GMCA/<版本> (<平台>)`。

这件事在这里是致命的，因为这类前置是**按"发起请求的那个 UA"给 CDN 直链签名**的：

```
INFO /play115[download_url] 即将调用 client.download_url: pickcode=… kwargs_keys=app,extensions,request,user_agent
INFO /play115[strm+ua] 缓存写入: canonical_strm=… ua=GMCA/1.4.0 (NX)
```

——`user_agent` 被透传给网盘客户端，而解析结果的缓存键是 `(strm, user_agent)`。也就是说：**取直链用的 UA 和播放直链用的 UA 必须逐字节一致**；只在 mpv 上改成浏览器 UA 比不改更糟。

## 改动

### 一、远端数据源走 `PlaybackInfo`（`feat(jellyfin)`）

- `app/include/api/media/types.hpp` —— `Media::remote`，由 Jellyfin/Emby 的 `IsRemote` 解出（`api/jellyfin/types.hpp` 的 `parseMediaSource`）。
- `app/include/api/jellyfin/playback_info.hpp`（新增，纯逻辑、有单测）—— `directStreamUrl(response, sourceId, serverUrl, token)`：在 `MediaSources` 中选出与当前播放版本匹配的那一项，返回其 `DirectStreamUrl`；没有可用地址时返回 `""`：
  - 相对地址、非 `http(s)` 一律忽略；
  - 回环 / RFC1918 / `169.254` / `.local` 主机忽略——**除非**它就是服务器自己的 host。那是反代后面服务器上报的内网地址，另一张网里的主机访问不到；拒绝它可以保证这些部署维持原有行为；
  - 当地址是服务器自身 host 且不含 `api_key` 时，补上 token（原生 Emby 通常自带，上述前置不带）。
- `app/src/api/jellyfin/backend.cpp` —— `resolvePlayback()` **仅当所选数据源是远端**时才问 `PlaybackInfo`；无论因何失败（网络错误、没有该字段、地址不可用）都回退到原先拼出来的 URL。普通条目完全走原路径，这个额外请求**永远不会**让播放失败。
- `tests/test_jellyfin_playback.cpp` —— 覆盖上面的真实响应形状、数据源选择、内网/相对/畸形响应的拒绝、token 追加。

转码/HLS 分支与下载地址仍然使用原先拼出来的 URL，本次未触及。

### 二、全 App 一个 User-Agent（`fix(http)`）

- `HTTP::default_user_agent()` 成为唯一事实来源；新增 `HTTP::set_default_user_agent()` 与 `HTTP::builtin_user_agent()`（空设置时回落到内置的 `<package>/<version> (<platform>)`）——`app/include/api/http.hpp`、`app/src/api/http.cpp`。
- `AppConfig::init()` 一次性解析：`player_user_agent`（config.json / 设置页）→ `{configDir}/mpv.conf` 里的 `user-agent=` → 内置值。`utils/mpv_conf.hpp` 补了 `lineValue()`/`value()`（此前只能回答"文件里有没有这一项"，无法取出值）；`tests/test_mpv_conf.cpp` 覆盖之。
- `app/src/view/mpv_core.cpp` 现在始终把 `HTTP::default_user_agent()` 原样交给 mpv；播放器侧那份副本已删除，两边不可能再漂移。
- 设置 → 网络 → "自定义 User-Agent"（`setting_tab.cpp`，14 种语言）写入设置、同步 HTTP 客户端，并在播放器已存在时重启它。清空即回到内置 UA。
- `app/src/client/client.cpp` —— 远程库的单独覆盖写的是 `user_agent=`，mpv 会静默丢弃：选项名是逐字节匹配的（`m_config_get_co_raw()` 用 `bstrcmp`），所以此前只有 curl 那一半生效。改为 `user-agent=`（带引号、转义）。

## 验证

- 改动后前置日志：`PlaybackInfo 处理成功: path=/Items/1223/PlaybackInfo, direct=1`，`ua=GMCA/1.4.0 (NX)`；随后客户端打开 CDN 地址并开始播放。mp4 与 mkv 两个条目均确认。
- `tests/test_mpv_conf.cpp` 与 `tests/test_jellyfin_playback.cpp` 是独立测试（`tests/run.sh`，不接 CMake）。
- CI（`build`，`platform=nx`）能用这些改动编出 NRO。

## 说明与后续

- `IsRemote` 这个门槛意味着：不上报该标记的服务器仍走旧路径。若确有部署需要，把触发条件放宽成"直连播放时总是问一次 PlaybackInfo"只需一行改动，代价是每次播放多一个请求。
- 建议下一步：转码/HLS 分支和 `downloadUrl()` 做同样处理，它们目前仍分发拼出来的 URL。
- `mpv.conf` 里的 `user-agent=` 只在启动时读一次（它是进程级唯一值），改文件需要重启 App；设置页改则立即生效。
