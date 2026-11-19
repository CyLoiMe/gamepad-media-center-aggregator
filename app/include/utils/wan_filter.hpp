/*
    Which libraries a connection may show while it is on its metered address.

    `AppUser::wan_visible_libraries` is a per-profile allow-list: two accounts on
    one server see different libraries, so it cannot live on the server entry.
    "Not configured" (std::nullopt upstream) keeps the historical behaviour —
    nothing hidden — while a configured list is authoritative, so an empty one
    hides everything. That asymmetry is deliberate: a fresh upgrade must not
    present an empty app, and a configured list must be obeyed exactly.

    The predicate takes a pointer rather than the optional itself so this header
    stays usable from plain C++11 (tests compile it without the app's C++17
    toolchain); `nullptr` is the "not configured" case. The backends snapshot the
    list on the UI thread (AppConfig::allowedLibraries) and filter with it on the
    worker.
*/

#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace wanfilter {

/// May `libraryId` be shown under `allow`? `allow == nullptr` = no filtering.
inline bool allows(const std::vector<std::string>* allow, const std::string& libraryId) {
    if (allow == nullptr) return true;
    // Filtering is active and the item does not name its library: hide it rather
    // than risk surfacing something from a library the user excluded.
    if (libraryId.empty()) return false;
    return std::find(allow->begin(), allow->end(), libraryId) != allow->end();
}

}  // namespace wanfilter
