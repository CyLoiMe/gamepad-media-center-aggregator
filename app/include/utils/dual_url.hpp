/*
    A connection that knows two addresses: one on the home network, one reachable
    from outside (FRP/tunnel or a public host), plus the policy that decides how
    they are raced.

    The probing itself is the existing `plex::raceConnections()` (priority
    ordered) or `plex::raceFirst()` (first answer wins). Both run off the UI
    thread. This header is the pure part — which candidates to hand over, what the
    answer means, and which policy the stored string names — so
    tests/test_dual_url.cpp covers it without a server.

    Policies:
      lan_first (default) — the LAN address is probed first and wins as soon as it
        answers; the remote one only takes over once the LAN probe has FAILED.
        A faster WAN answer never preempts a reachable LAN address, which is the
        safe default for a metered tunnel.
      lan_only — only the LAN address is probed. For a console that never leaves
        the house: no packet can ever reach the tunnel. Unreachable when roaming,
        until the user switches the policy (or forces the address in the network
        dialog).
      race — every candidate is probed at once and the FIRST answer wins. Fastest
        connection, but at home the tunnel can win the race and burn quota; opt-in
        only.
*/

#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace dualurl {

enum class Policy { LanFirst, LanOnly, Race };

/// Stored string -> policy. Anything unknown (including an empty value from a
/// config predating this setting) means LanFirst: the conservative choice.
inline Policy policyOf(const std::string& name) {
    if (name == "lan_only") return Policy::LanOnly;
    if (name == "race") return Policy::Race;
    return Policy::LanFirst;
}

/// Probe candidates: the LAN address first (when set), then the remote list in
/// its stored priority order. Empty strings are skipped and duplicates dropped
/// (a user may paste the same address in both fields).
inline std::vector<std::string> candidates(
    const std::string& localUrl, const std::vector<std::string>& remoteUrls) {
    std::vector<std::string> out;
    auto add = [&out](const std::string& url) {
        if (url.empty()) return;
        if (std::find(out.begin(), out.end(), url) != out.end()) return;
        out.push_back(url);
    };
    add(localUrl);
    for (const auto& url : remoteUrls) add(url);
    return out;
}

/// Candidates under a policy. LanOnly drops the remote addresses entirely (unless
/// there is no LAN address at all, in which case it degrades to them).
inline std::vector<std::string> candidates(
    Policy policy, const std::string& localUrl, const std::vector<std::string>& remoteUrls) {
    if (policy == Policy::LanOnly && !localUrl.empty()) return candidates(localUrl, {});
    return candidates(localUrl, remoteUrls);
}

/// Did the probe land on the home address? A connection without a LAN address
/// has no LAN mode at all, whatever answered.
inline bool isLan(const std::string& chosenUrl, const std::string& localUrl) {
    return !localUrl.empty() && !chosenUrl.empty() && chosenUrl == localUrl;
}

}  // namespace dualurl
