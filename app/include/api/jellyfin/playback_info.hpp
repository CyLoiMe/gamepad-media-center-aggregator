/*
    Jellyfin/Emby PlaybackInfo: the play URL the SERVER hands out.

    Most items play fine from the URL we assemble ourselves
    (/Videos/{id}/stream?static=true&mediaSourceId=…). Remote sources (`.strm`
    pointing at a cloud drive — 115, 123, 夸克…) can be different: front-ends
    such as TgtoDrive answer PlaybackInfo with `DirectStreamUrl` rewritten to the
    cloud CDN's signed link and **reject** our own URL (403 "缺少 Emby UserId"),
    so the URL from the response is the only one that plays.

    Pure logic (JSON + strings), so tests/test_jellyfin_playback.cpp covers it.
*/

#pragma once

#include <cctype>
#include <string>

#include <nlohmann/json.hpp>

#include "api/media/types.hpp"
#include "utils/hostname.hpp"

namespace jellyfin {

namespace detail {

/// Link-local / loopback / RFC1918 literals: the address a server reports for
/// itself, which a console on another network cannot reach.
inline bool isPrivateIpv4(const std::string& host) {
    int octet[4] = {0, 0, 0, 0};
    int dots = 0, digits = 0, value = 0;
    for (char c : host) {
        if (c == '.') {
            if (digits == 0 || dots >= 3) return false;
            octet[dots++] = value;
            value = digits = 0;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            if (++digits > 3) return false;
            value = value * 10 + (c - '0');
        } else {
            return false;  // not a numeric IPv4 literal
        }
    }
    if (dots != 3 || digits == 0) return false;
    octet[3] = value;
    return octet[0] == 10 || octet[0] == 127 || (octet[0] == 192 && octet[1] == 168) ||
           (octet[0] == 172 && octet[1] >= 16 && octet[1] <= 31) || (octet[0] == 169 && octet[1] == 254);
}

/// Host the console cannot reach: a private address, loopback or an mDNS name.
inline bool isInternalHost(const std::string& url) {
    std::string host = net::hostFromUrl(url);
    for (char& c : host) c = (char)std::tolower(static_cast<unsigned char>(c));
    if (host.empty() || host == "localhost" || host == "::1") return true;
    return net::isMdnsHost(url) || isPrivateIpv4(host);
}

}  // namespace detail

/// `DirectStreamUrl` from a PlaybackInfo response, or "" when there is none the
/// console can actually play (the caller then keeps its own URL).
///
/// `sourceId` selects the matching MediaSources entry (falls back to the first);
/// `serverUrl` + `token` are only used to recognise the server's own host, whose
/// URL needs the token appended when the server left it out.
inline std::string directStreamUrl(const nlohmann::json& resp, const std::string& sourceId,
    const std::string& serverUrl, const std::string& token) {
    auto sources = resp.find("MediaSources");
    if (sources == resp.end() || !sources->is_array()) return "";

    const nlohmann::json* picked = nullptr;
    for (const auto& source : *sources) {
        if (!source.is_object()) continue;
        if (!picked) picked = &source;  // first one, when sourceId is unknown here
        if (!sourceId.empty() && media::jstr(source, "Id") == sourceId) {
            picked = &source;
            break;
        }
    }
    if (picked == nullptr) return "";

    std::string url = media::jstr(*picked, "DirectStreamUrl");
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) return "";

    const bool ownHost = net::hostFromUrl(url) == net::hostFromUrl(serverUrl);
    if (!ownHost && detail::isInternalHost(url)) return "";  // server-internal address

    if (ownHost && !token.empty() && url.find("api_key=") == std::string::npos)
        url += (url.find('?') == std::string::npos ? "?" : "&") + std::string("api_key=") + token;
    return url;
}

}  // namespace jellyfin
