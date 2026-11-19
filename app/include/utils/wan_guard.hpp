/*
    Last line of defence before mpv is handed a URL.

    Filtering the library lists keeps hidden content out of the UI, but an item
    can still reach the player from a cross-library surface (favorites, a
    playlist, a deep link). On the metered address that would start pulling video
    through the tunnel within seconds, so playback refuses it unless the item's
    library is explicitly allowed.

    The rule:
      - not filtering (LAN, or no allow-list configured) -> play;
      - library known and allowed -> play, even when the URL is the server's own
        (the user asked for that library on WAN; its bytes are their call);
      - library known and hidden -> refuse;
      - library unknown -> refuse unless the resolved URL is external (a cloud
        CDN direct link), because nothing proves the item is allowed.

    `isTunnelUrl()` answers "would these bytes cross the tunnel?" by host: the
    server's own host means yes. It is used for the unknown-library case only —
    a fast 302 to a CDN leaves the server host behind, which is exactly the
    "cheap on quota" case the spec describes.

    Pure: tests/test_wan_guard.cpp covers it.
*/

#pragma once

#include <algorithm>
#include <cctype>
#include <string>

#include "utils/hostname.hpp"

namespace wanguard {

namespace detail {

inline std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

}  // namespace detail

/// True when `url` is served by `serverUrl`'s host, i.e. the bytes cross the
/// tunnel. Host-only comparison: scheme, port and path do not matter, and an
/// unparsable host never counts as a match.
inline bool isTunnelUrl(const std::string& serverUrl, const std::string& url) {
    const std::string server = detail::lower(net::hostFromUrl(serverUrl));
    const std::string host = detail::lower(net::hostFromUrl(url));
    return !server.empty() && !host.empty() && server == host;
}

/// Must playback be refused? See the rule in the header comment.
inline bool blocksPlayback(bool filtering, bool libraryKnown, bool libraryAllowed, bool tunnelUrl) {
    if (!filtering) return false;
    if (libraryKnown) return !libraryAllowed;
    return tunnelUrl;
}

}  // namespace wanguard
