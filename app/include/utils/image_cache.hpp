#pragma once

/*
    ImageCache — on-disk cache of artwork so fiches render without the server
    (SPEC §4.2). Keyed by the raw path/URL passed to Image::load (a relative
    Plex path such as item.thumb/art/clearLogo, or an absolute http URL such as
    a tmdb cast face). Files live under {config}/downloads/art/{key}.

    A relative path is the SERVER's own id (/library/metadata/123/thumb/…,
    /Items/{guid}/Images/Primary), and Plex numbers libraries and metadata per
    server: the same path names a different picture on another one. The key is
    therefore scoped by the connection that fetched it (AppConfig::activeServerId);
    absolute URLs already carry their host and are left alone. Offline there is
    nothing to re-fetch, so a copy cached under any configured server is accepted —
    a downloaded item must keep its artwork when another connection is the active
    one, which is exactly the state the offline shell starts in.

    A locally cached asset is preferred by Image::load even online, giving
    downloaded content instant local artwork (SPEC AC6/AC17).
*/

#include <cstdint>
#include <cstdio>
#include <string>

namespace ImageCache {

/// Stable, cross-run 64-bit FNV-1a of the input as 16 lowercase hex chars.
/// Must NOT use std::hash (which is not stable across runs) — the key names a
/// file persisted to disk.
inline std::string key(const std::string& s) {
    uint64_t h = 0xcbf29ce484222325ULL;  // FNV offset basis
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001b3ULL;  // FNV prime
    }
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)h);
    return std::string(buf);
}

/// Populate the in-memory index of cached keys by scanning the art dir once.
/// Call at startup — keeps has() off the disk on the UI thread.
void init();

/// {config}/downloads/art
std::string dir();
/// absolute local file where this asset is (or would be) cached.
std::string localPath(const std::string& pathOrUrl);
/// a cached copy exists on disk.
bool has(const std::string& pathOrUrl);
/// fetch the asset (server for a relative path, direct for an absolute URL) and
/// store it locally. Best-effort, blocking — call off the UI thread.
bool store(const std::string& pathOrUrl);
/// remove a cached asset (no-op if absent).
void remove(const std::string& pathOrUrl);

}  // namespace ImageCache
