// Standalone logic test — the guard that protects the tunnel quota.
//
// Filtering the UI is not enough: an item can reach the player from a
// cross-library surface (favorites, playlist, deep link), and a download pulls
// the whole file through whatever address is active. See utils/wan_guard.hpp.

#include <cstdio>
#include <string>

#include <utils/wan_guard.hpp>

static int failures = 0;
#define CHECK(cond)                                          \
    do {                                                     \
        if (!(cond)) {                                       \
            printf("FAIL: %s (line %d)\n", #cond, __LINE__);  \
            ++failures;                                      \
        }                                                    \
    } while (0)

int main() {
    // Not filtering (home network, or no allow-list): never block.
    CHECK(!wanguard::blocksTransfer(false, false, false, true));
    CHECK(!wanguard::blocksTransfer(false, true, false, true));

    // Library known and allowed: play, even when the bytes come from the server
    // itself (the user explicitly allowed this library on WAN).
    CHECK(!wanguard::blocksTransfer(true, true, true, true));
    CHECK(!wanguard::blocksTransfer(true, true, true, false));

    // Library known and hidden: refuse, whatever the URL is.
    CHECK(wanguard::blocksTransfer(true, true, false, false));
    CHECK(wanguard::blocksTransfer(true, true, false, true));

    // Library unknown: only an EXTERNAL link (a CDN direct link) gets through.
    CHECK(wanguard::blocksTransfer(true, false, false, true));
    CHECK(!wanguard::blocksTransfer(true, false, false, false));

    // Tunnel detection is host-only: scheme, port and path are irrelevant.
    CHECK(wanguard::isTunnelUrl("http://emby.example.com:8091", "http://emby.example.com:8091/Videos/1/stream"));
    CHECK(wanguard::isTunnelUrl("https://Emby.Example.com", "http://emby.example.com:443/x?y=1"));
    CHECK(!wanguard::isTunnelUrl("http://emby.example.com:8091", "https://cdn.115.com/video.mp4"));

    // A different port is the same host: the tunnel is identified by host, and a
    // server reached on another port is still the server.
    CHECK(wanguard::isTunnelUrl("http://emby.example.com:8091", "http://emby.example.com:8096/stream"));

    // Unparsable / empty hosts never count as a match.
    CHECK(!wanguard::isTunnelUrl("", "http://emby.example.com"));
    CHECK(!wanguard::isTunnelUrl("http://emby.example.com", ""));

    if (failures == 0) printf("test_wan_guard: OK\n");
    return failures == 0 ? 0 : 1;
}
