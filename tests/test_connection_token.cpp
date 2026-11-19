// Standalone logic test — which token authenticates the active connection.
//
// Regression guard for "two Emby accounts on one server show the same library":
// Jellyfin/Emby issue one AccessToken per ACCOUNT, but both accounts share one
// AppServer entry (keyed by the server Id), so its single `access_token` slot
// only ever holds the last account that authenticated. Authentication must come
// from the active profile there, and from the server for Plex/Stremio.
//
// See utils/connection_token.hpp.

#include <cstdio>
#include <string>

#include <utils/connection_token.hpp>

static int failures = 0;
#define CHECK(cond)                                          \
    do {                                                     \
        if (!(cond)) {                                       \
            printf("FAIL: %s (line %d)\n", #cond, __LINE__);  \
            ++failures;                                      \
        }                                                    \
    } while (0)

int main() {
    // Emby/Jellyfin: the profile's own token wins, even though the shared server
    // slot holds the other account's.
    CHECK(connection::activeToken("emby", "token-from-the-other-account", "mine") == "mine");
    CHECK(connection::activeToken("jellyfin", "token-from-the-other-account", "mine") == "mine");
    // ... and falls back to the server slot when a config predates the user token.
    CHECK(connection::activeToken("emby", "server-only", "") == "server-only");
    CHECK(connection::activeToken("jellyfin", "server-only", "") == "server-only");

    // Plex: the SERVER token opens the PMS; the profile's plex.tv token cannot.
    CHECK(connection::activeToken("plex", "server", "plex-tv-account-token") == "server");
    CHECK(connection::activeToken("plex", "", "plex-tv-account-token") == "plex-tv-account-token");

    // Stremio: the authKey, stored on both sides.
    CHECK(connection::activeToken("stremio", "authkey", "authkey") == "authkey");
    CHECK(connection::activeToken("stremio", "authkey", "") == "authkey");

    // Unknown/default type behaves like Plex (configs written before multi-backend).
    CHECK(connection::activeToken("", "server", "user") == "server");

    if (failures == 0) printf("test_connection_token: OK\n");
    return failures == 0 ? 0 : 1;
}
