/*
    Pure logic of an in-place connection edit (see AppConfig::updateConnection).

    Editing must never cost the user what the sign-in form cannot carry. A
    re-login can hand back new ids (a restored Jellyfin server issues a new
    server Id, a recreated account a new user Id), and the pieces that belong to
    the PROFILE — its WAN library allow-list, its sidebar layout — are keyed by
    those ids. This header decides what survives and what is dropped, so the rule
    can be tested without borealis, HTTP or a config file.

    Lives under app/include/utils/ so tests/run.sh compiles it standalone.
*/

#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace connection_edit {

/// Keep what the edit form could not express. The form builds a fresh profile
/// from the login response; the WAN allow-list is not a field there, so losing it
/// here would silently reopen every hidden library on the tunnel. `hasLibraries`
/// / `libraries` model `AppUser::wan_visible_libraries` (engaged vs. not) without
/// requiring <optional> here, so this header stays compilable on a C++11 box.
template <typename Profile>
inline void keepProfileSettings(Profile& edited, const Profile& stored) {
    if (!edited.hasLibraries) {
        edited.hasLibraries = stored.hasLibraries;
        edited.libraries = stored.libraries;
    }
}

/// Is the old server entry still needed? Only while another profile points at it.
/// The edited profile is passed too: it must already point at the NEW id, or a
/// same-id edit would look like a reference to itself.
template <typename Profile>
inline bool serverStillReferenced(const std::vector<Profile>& profiles, const std::string& oldServerId) {
    if (oldServerId.empty()) return false;
    return std::any_of(profiles.begin(), profiles.end(),
        [&oldServerId](const Profile& p) { return p.server_id == oldServerId; });
}

/// Should the entry stored under `oldId` be dropped after the edit? Only when the
/// id actually changed — a same-id edit rewrites the entry in place.
inline bool dropOldEntry(const std::string& oldId, const std::string& newId, bool stillReferenced) {
    return !oldId.empty() && oldId != newId && !stillReferenced;
}

}  // namespace connection_edit
