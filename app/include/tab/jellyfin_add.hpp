/*
    SMCA — sign-in form for a Jellyfin or Emby server.
    URL + username/password login. The backend type ("jellyfin" | "emby") only
    changes the title and the value stored in the saved AppServer. On success
    the active server/user are persisted and the app enters MainActivity.

    Reached from ServerTypeChoose (the Jellyfin/Emby cell present()s this view),
    so it is a content view of the ServerList AppletFrame: it keeps the footer
    and B (the frame's hints/back action) returns to the type chooser.

    The same form EDITS an existing connection (pencil on the tile). Editing must
    never cost the user their setup, so:
      - the fields start pre-filled;
      - a blank password with an unchanged username changes only the addresses:
        the stored token still authenticates, no re-login, no new profile;
      - a real re-login goes through AppConfig::updateConnection(), which keeps
        the WAN library allow-list and the sidebar layout even when the server or
        user Id comes back different.
*/

#pragma once

#include <borealis.hpp>
#include "utils/config.hpp"
#include <functional>

namespace jellyfin {
struct LoginResult;
}

class JellyfinAdd : public brls::Box {
public:
    /// `type` is "jellyfin" or "emby". Without a connection this adds a new one;
    /// with one it edits that connection in place (`onSaved` runs after a
    /// successful save that is not the active connection).
    explicit JellyfinAdd(const std::string& type);
    JellyfinAdd(const std::string& type, const AppUser& user, const AppServer& server,
        std::function<void()> onSaved = {});
    ~JellyfinAdd() override;

    /// Focus the URL field on entry (deterministic, no phantom focus).
    brls::View* getDefaultFocus() override;

private:
    /// Validates the URL, authenticates (async + spinner) then saves.
    void submit();
    /// Persists a new connection and enters the application.
    void finish(const std::string& remote, const std::string& local,
        const std::string& active, bool lan, const jellyfin::LoginResult& r);
    /// Persists an edit (see updateConnection) and leaves the form.
    void finishEdit(const std::string& remote, const std::string& local,
        const std::string& active, bool lan, const jellyfin::LoginResult& r);

    BRLS_BIND(brls::Label, header, "jellyfin/add/header");
    BRLS_BIND(brls::InputCell, cellUrl, "jellyfin/add/url");
    /// Optional LAN address (probed before the remote one). Empty = single address.
    BRLS_BIND(brls::InputCell, cellLocal, "jellyfin/add/local_url");
    BRLS_BIND(brls::InputCell, cellUser, "jellyfin/add/user");
    BRLS_BIND(brls::InputCell, cellPasswd, "jellyfin/add/passwd");
    BRLS_BIND(brls::Button, btnConnect, "jellyfin/add/connect");
    BRLS_BIND(brls::ProgressSpinner, spinner, "jellyfin/add/spinner");
    BRLS_BIND(brls::Label, labelStatus, "jellyfin/add/status");

    std::string type;

    /// Edit mode: the connection being replaced, its stored profile (the token
    /// that survives an address-only save) and the caller's refresh hook.
    bool editing = false;
    std::string oldServerId;
    std::string oldUserId;
    std::string oldUserName;
    std::string oldServerName;
    AppServer oldServer;
    AppUser oldUser;
    std::function<void()> onSaved;
};
