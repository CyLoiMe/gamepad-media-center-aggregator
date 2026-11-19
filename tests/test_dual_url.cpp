// Standalone logic test — dual-address probe ordering and LAN detection.
//
// A connection can declare a home address and one or more remote ones. The probe
// races them with the LAN address first, so a console at home stays off the
// tunnel (no bandwidth quota) and falls back to the remote address elsewhere.
// See utils/dual_url.hpp.

#include <cstdio>
#include <string>
#include <vector>

#include <utils/dual_url.hpp>

static int failures = 0;
#define CHECK(cond)                                          \
    do {                                                     \
        if (!(cond)) {                                       \
            printf("FAIL: %s (line %d)\n", #cond, __LINE__);  \
            ++failures;                                      \
        }                                                    \
    } while (0)

int main() {
    // LAN address first, remote candidates in their stored order.
    auto c = dualurl::candidates("http://192.168.1.100:8091", {"http://emby.example.com:8091"});
    CHECK(c.size() == 2);
    CHECK(c[0] == "http://192.168.1.100:8091");
    CHECK(c[1] == "http://emby.example.com:8091");

    // A single-address connection keeps exactly its remote candidate.
    auto only = dualurl::candidates("", {"http://emby.example.com:8091", "https://relay.example.com"});
    CHECK(only.size() == 2);
    CHECK(only[0] == "http://emby.example.com:8091");

    // The same address pasted in both fields must not be probed twice.
    auto dup = dualurl::candidates("http://host:8091", {"http://host:8091", "https://other:443"});
    CHECK(dup.size() == 2);
    CHECK(dup[0] == "http://host:8091");
    CHECK(dup[1] == "https://other:443");

    // Nothing configured at all.
    CHECK(dualurl::candidates("", {}).empty());

    // LAN mode is exactly "the LAN address answered".
    CHECK(dualurl::isLan("http://192.168.1.100:8091", "http://192.168.1.100:8091"));
    CHECK(!dualurl::isLan("http://emby.example.com:8091", "http://192.168.1.100:8091"));
    // No LAN address configured -> never LAN, whatever answered.
    CHECK(!dualurl::isLan("http://emby.example.com:8091", ""));
    // Nothing answered -> not LAN.
    CHECK(!dualurl::isLan("", "http://192.168.1.100:8091"));

    // Policy names: unknown or empty (config written before the setting existed)
    // must fall back to the safe, strictly-priority behaviour.
    CHECK(dualurl::policyOf("lan_first") == dualurl::Policy::LanFirst);
    CHECK(dualurl::policyOf("lan_only") == dualurl::Policy::LanOnly);
    CHECK(dualurl::policyOf("race") == dualurl::Policy::Race);
    CHECK(dualurl::policyOf("") == dualurl::Policy::LanFirst);
    CHECK(dualurl::policyOf("nonsense") == dualurl::Policy::LanFirst);

    // lan_only never hands the tunnel to the prober while a LAN address exists.
    auto homeOnly = dualurl::candidates(
        dualurl::Policy::LanOnly, "http://192.168.1.100:8091", {"http://emby.example.com:8091"});
    CHECK(homeOnly.size() == 1);
    CHECK(homeOnly[0] == "http://192.168.1.100:8091");
    // ... but degrades to the remote list when no LAN address was configured.
    auto homeOnlyNoLan = dualurl::candidates(dualurl::Policy::LanOnly, "", {"http://emby.example.com:8091"});
    CHECK(homeOnlyNoLan.size() == 1);
    CHECK(homeOnlyNoLan[0] == "http://emby.example.com:8091");

    // lan_first and race probe both, LAN first (they differ in HOW the race ends,
    // not in the candidate list).
    for (auto policy : {dualurl::Policy::LanFirst, dualurl::Policy::Race}) {
        auto both = dualurl::candidates(
            policy, "http://192.168.1.100:8091", {"http://emby.example.com:8091"});
        CHECK(both.size() == 2);
        CHECK(both[0] == "http://192.168.1.100:8091");
        CHECK(both[1] == "http://emby.example.com:8091");
    }

    if (failures == 0) printf("test_dual_url: OK\n");
    return failures == 0 ? 0 : 1;
}
