/*
    Reading the user's mpv.conf: which User-Agent does it configure, if any?

    The app sends ONE User-Agent everywhere — its own HTTP client (API, artwork,
    downloads) and mpv's stream layer both use HTTP::default_user_agent(). That is
    not cosmetic: cloud-drive front-ends (TgtoDrive and the like) sign the URL they
    hand out with the UA of the request that asked for it, so a client playing with
    a different UA gets a 403 from the CDN.

    Hence this lane: `player_user_agent` (config.json / settings tab) wins, then a
    `user-agent=` line in {configDir}/mpv.conf, then the built-in UA. Both knobs
    end up in the same place (AppConfig::init()), so the two sides can never
    disagree.

    Handled syntax: "name=value", an optional leading "--", '#' starts a comment,
    blanks around the name tolerated, quotes around the value stripped. Option
    names are matched byte for byte, like mpv does ('_' is NOT a separator:
    m_config_get_co_raw() compares with bstrcmp).

    A flat scan: a UA set from an `include=`d file is not seen, and the built-in
    default applies there.
*/

#pragma once

#include <cstddef>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace mpvconf {

namespace detail {

/// Both ends trimmed, a CR from a CRLF file included.
inline std::string_view trim(std::string_view s) {
    constexpr std::string_view blanks = " \t\r";
    const size_t first = s.find_first_not_of(blanks);
    if (first == std::string_view::npos) return {};
    return s.substr(first, s.find_last_not_of(blanks) - first + 1);
}

}  // namespace detail

/// Value of `<key>=…` on one mpv.conf line, or std::nullopt when the line sets
/// something else (or is commented out).
inline std::optional<std::string> lineValue(std::string_view line, std::string_view key) {
    if (const size_t hash = line.find('#'); hash != std::string_view::npos) line = line.substr(0, hash);
    line = detail::trim(line);
    if (line.substr(0, 2) == "--") line.remove_prefix(2);
    // "name = value": the name decides, and a bare "name" (no '=') toggles an
    // OPT_FLAG instead of setting the string option we are looking for.
    const size_t eq = line.find('=');
    if (eq == std::string_view::npos) return std::nullopt;
    if (detail::trim(line.substr(0, eq)) != key) return std::nullopt;

    std::string_view val = detail::trim(line.substr(eq + 1));
    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') val = val.substr(1, val.size() - 2);
    return std::string(val);
}

/// Value of `key` in {configDir}/mpv.conf (first line that sets it), or
/// std::nullopt when the file is missing or does not set it.
inline std::optional<std::string> value(const std::string& configDir, std::string_view key) {
    std::ifstream in(configDir + "/mpv.conf", std::ios::binary);
    if (!in) return std::nullopt;
    std::string line;
    size_t budget = 64 * 1024;  // a config file, not a payload
    while (std::getline(in, line)) {
        if (line.size() + 1 >= budget) break;
        budget -= line.size() + 1;
        if (auto v = lineValue(line, key)) return v;
    }
    return std::nullopt;
}

}  // namespace mpvconf
