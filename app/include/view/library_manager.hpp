/*
    SMCA — libraries manager (issue #9).

    A full-content detail view (presented over the main content area while the
    sidebar stays visible, so every change previews live) that reorders and
    shows/hides the reorderable sidebar tabs: server libraries + Playlists +
    Watchlist. The layout is saved per server.

    Gamepad reorder uses a "grab" mode (the sidebar list can't use D-pad
    up/down for both navigation and moving, and LB/RB are already the tab
    switchers on the frame):
      - A       : grab / drop the focused row
      - D-pad ▲▼: move the row while grabbed
      - Y       : show / hide the focused row in the sidebar (when not grabbed)
      - X       : allow / hide the focused library on WAN (Jellyfin/Emby only)
      - B       : back (registered by the detail-view host)

    The X column is the WAN visibility matrix: this screen already lists exactly
    the libraries of the active connection, so a second per-row state is all the
    matrix needs — no separate screen to keep in sync with the sidebar. It edits
    `AppUser::wan_visible_libraries` (utils/wan_filter.hpp), which the backends
    read through AppConfig::allowedLibraries().
*/

#pragma once

#include <borealis.hpp>
#include "activity/main_activity.hpp"
#include <optional>
#include <set>
#include <string>
#include <vector>

class LibraryManager : public brls::Box {
public:
    explicit LibraryManager(MainTabFrame* frame);

    brls::View* getDefaultFocus() override;

    // --- called by the rows ---
    bool isGrabbed(int index) const { return this->grabbedIndex == index; }
    bool anyGrabbed() const { return this->grabbedIndex >= 0; }
    void toggleGrab(int index);
    void moveGrabbed(int delta);
    void toggleVisible(int index);

    /// Is this library allowed on WAN? (true when nothing is configured yet)
    bool wanShown(const std::string& id) const;
    /// Libraries carry a WAN flag only on the backends that enforce it.
    bool wanCapable() const { return this->wanEnabled; }
    /// Flip the focused library's WAN visibility and persist it.
    void toggleWan(int index);

private:
    void rebuild();
    void commit();  // persist the working order/visibility + apply live

    MainTabFrame* frame;
    std::vector<MainTabFrame::SidebarEntry> entries;  // working copy
    std::optional<std::vector<std::string>> wanLibs;  // stored allow-list, live copy
    bool wanEnabled = false;  // false for backends without a WAN filter
    int grabbedIndex = -1;
    int pendingFocus = 0;  // row index to focus after the next rebuild
    bool live = false;     // false during the initial (pre-mount) build

    brls::Box* rowsBox = nullptr;
    brls::View* focusTarget = nullptr;
};
