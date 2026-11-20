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

/// The id that has to be resolved to reach the item's library, or "" when the
/// item's own ParentId already IS the library.
///
/// Jellyfin/Emby never put the library in ParentId for a hierarchy: an episode's
/// ParentId is its SEASON and a season's is its SHOW, and the mappers rewrite
/// ParentId to the artist (album) or the album (track) for music. Comparing any
/// of those with a library id would silently mismatch. The caller resolves the
/// returned id — /Items/{id}/Ancestors ends with the CollectionFolder that is the
/// library — so pass the item's own id as `id` for the self cases.
inline std::string libraryLookupId(const std::string& type, const std::string& id,
    const std::string& parentId, const std::string& seriesId) {
    if (type == "episode") return seriesId;  // ParentId is the season
    if (type == "season") return parentId;   // ParentId is the show
    if (type == "artist" || type == "album" || type == "track") return id;  // virtual / remapped parent
    return "";  // movie, show, clip, collection: ParentId IS the library
}

}  // namespace wanfilter
