/*
    Which stored token authenticates the active connection?

    The two are not interchangeable, and the difference is what made two accounts
    on one Emby server look like one:

      - Plex: the SERVER token opens a PMS. A profile's plex.tv token cannot, and
        one plex.tv profile backs several servers — so the token belongs to the
        AppServer entry.
      - Jellyfin / Emby: every account authenticates separately and gets its own
        AccessToken. An AppServer entry is keyed by the server Id, so two accounts
        on the same server SHARE one entry and its single `access_token` slot only
        holds whichever account authenticated last. Reading that slot
        authenticates the new selection as the old account.
      - Stremio: the account authKey, stored on both.

    Hence: for Jellyfin/Emby the active USER's token wins; for the others the
    SERVER token does. Both sides fall back to the other when their preferred one
    is empty (configs written before the user token was kept, or a server that
    never stored one).
*/

#pragma once

#include <string>

namespace connection {

/// The token for API requests against the active connection.
/// `backendType` is AppServer::type ("plex" | "jellyfin" | "emby" | "stremio").
inline std::string activeToken(const std::string& backendType,
    const std::string& serverToken, const std::string& userToken) {
    // Jellyfin/Emby = one token per account; the server-level slot cannot tell
    // two accounts on the same server apart.
    const bool perAccount = backendType == "jellyfin" || backendType == "emby";
    const std::string& primary = perAccount ? userToken : serverToken;
    const std::string& fallback = perAccount ? serverToken : userToken;
    return primary.empty() ? fallback : primary;
}

}  // namespace connection
