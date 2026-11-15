/*
    Reading the user's mpv.conf: does it configure an option itself?

    mpv gives an option set through mpv_set_option_string() the same precedence
    as a command-line option, and the command line overrides the config file
    (manual, FILES: "options given on the command line override either"). An
    option the app always sets therefore silently discards the user's value in
    {configDir}/mpv.conf — the app has to know when to stay silent instead.

    Handled syntax: "name=value", an optional leading "--", '#' starts a comment,
    blanks around the name tolerated. Option names are matched byte for byte, like
    mpv does ('_' is NOT a separator: m_config_get_co_raw() compares with bstrcmp,
    so "user_agent=" in mpv.conf is an unknown option for mpv too).

    A flat scan: an option set from an `include=`d file is not seen, and the app
    default applies there, exactly as before.
*/

#pragma once

#include <cstddef>
#include <fstream>
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

/// One mpv.conf line: is it `<key>=...`?
inline bool lineSets(std::string_view line, std::string_view key) {
    if (const size_t hash = line.find('#'); hash != std::string_view::npos) line = line.substr(0, hash);
    line = detail::trim(line);
    if (line.substr(0, 2) == "--") line.remove_prefix(2);
    // "name = value": only the name matters, and a bare "name" (no '=') toggles an
    // OPT_FLAG instead of setting the string option we are looking for.
    const size_t eq = line.find('=');
    if (eq == std::string_view::npos) return false;
    return detail::trim(line.substr(0, eq)) == key;
}

/// Does {configDir}/mpv.conf set `key`? False when the file does not exist.
inline bool sets(const std::string& configDir, std::string_view key) {
    std::ifstream in(configDir + "/mpv.conf", std::ios::binary);
    if (!in) return false;
    std::string line;
    size_t budget = 64 * 1024;  // a config file, not a payload
    while (std::getline(in, line)) {
        if (line.size() + 1 >= budget) break;
        budget -= line.size() + 1;
        if (lineSets(line, key)) return true;
    }
    return false;
}

}  // namespace mpvconf
