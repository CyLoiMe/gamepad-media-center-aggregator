/*
    LAN / WAN indicator for a connection that declares two addresses.

    The badge only exists when the active server has a LAN address configured
    (AppServer::local_url): a single-address server has no mode to report. It sits
    in the top-right corner of the content area and doubles as the entry point of
    the network dialog, opened with ZR (R is already the next-tab button).

    Nothing here probes on its own: `AppConfig::reachableUrl()` decided the mode
    when the connection was activated. The dialog can ask for a re-probe, which
    runs off the UI thread and only patches the active address + this badge.
*/

#pragma once

#include <borealis.hpp>

class NetworkBadge : public brls::Box {
public:
    NetworkBadge();

    /// True when the active connection has a LAN address, i.e. when the badge and
    /// the ZR action make sense.
    bool applicable() const;

    /// Re-read the active mode from AppConfig and repaint.
    void refresh();

    /// Status dialog: current mode/addresses, re-probe, manual switch.
    void showDialog();

private:
    void probe();
    void forceSwitch();

    brls::Label* text = nullptr;
};
