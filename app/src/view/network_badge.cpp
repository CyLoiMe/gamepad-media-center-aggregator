#include "view/network_badge.hpp"

#include <string>

#include "utils/config.hpp"
#include "utils/dual_url.hpp"

using namespace brls::literals;  // for _i18n

namespace {

/// Copy of the active server, safe to hand to a worker thread.
AppServer activeServer() {
    auto& conf = AppConfig::instance();
    if (conf.getUserId().empty()) return {};
    const std::string& id = conf.getUser().server_id;
    for (const auto& s : conf.getServers())
        if (s.id == id) return s;
    return {};
}

std::string summary() {
    auto& conf = AppConfig::instance();
    std::string out = conf.isLanMode() ? "main/network/lan"_i18n : "main/network/wan"_i18n;
    out += "\n";
    out += "main/network/active"_i18n;
    out += "\n";
    out += conf.getUrl();
    std::string local = conf.getLocalUrl();
    if (!local.empty()) {
        out += "\n";
        out += "main/network/local_addr"_i18n;
        out += "\n";
        out += local;
    }
    // Which fallback policy is in force — the thing that decides whether a failing
    // LAN probe may spend tunnel quota.
    switch (dualurl::policyOf(AppConfig::wan_policy)) {
        case dualurl::Policy::LanOnly:
            out += "\n";
            out += "main/setting/network/wan_policy/header"_i18n;
            out += ": ";
            out += "main/setting/network/wan_policy/lan_only"_i18n;
            break;
        case dualurl::Policy::Race:
            out += "\n";
            out += "main/setting/network/wan_policy/header"_i18n;
            out += ": ";
            out += "main/setting/network/wan_policy/race"_i18n;
            break;
        case dualurl::Policy::LanFirst:
            out += "\n";
            out += "main/setting/network/wan_policy/header"_i18n;
            out += ": ";
            out += "main/setting/network/wan_policy/lan_first"_i18n;
            break;
    }
    return out;
}

}  // namespace

NetworkBadge::NetworkBadge() {
    this->setAxis(brls::Axis::ROW);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setPaddingTop(5);
    this->setPaddingBottom(5);
    this->setPaddingLeft(12);
    this->setPaddingRight(12);
    this->setCornerRadius(13);
    this->setPositionType(brls::PositionType::ABSOLUTE);
    this->setPositionTop(12);
    this->setPositionRight(22);
    this->setFocusable(false);
    this->setAlpha(0.9f);

    this->text = new brls::Label();
    this->text->setFontSize(16);
    this->addView(this->text);

    this->refresh();
}

bool NetworkBadge::applicable() const { return !AppConfig::instance().getLocalUrl().empty(); }

void NetworkBadge::refresh() {
    if (!this->applicable()) {
        this->setVisibility(brls::Visibility::GONE);
        return;
    }
    this->setVisibility(brls::Visibility::VISIBLE);

    auto theme = brls::Application::getTheme();
    const bool lan = AppConfig::instance().isLanMode();
    this->text->setText(lan ? "LAN" : "WAN");
    this->text->setTextColor(lan ? nvgRGB(0x4C, 0xC3, 0x8A) : theme.getColor("color/app"));
    this->setBackgroundColor(theme.getColor("color/grey_1"));
}

void NetworkBadge::showDialog() {
    auto* dialog = new brls::Dialog(summary());
    dialog->addButton("hints/cancel"_i18n, []() {});
    dialog->addButton("main/network/reprobe"_i18n, [this]() { this->probe(); });
    dialog->addButton("main/network/force"_i18n, [this]() { this->forceSwitch(); });
    dialog->open();
}

void NetworkBadge::probe() {
    AppServer target = activeServer();
    if (target.id.empty()) return;

    ASYNC_RETAIN
    brls::async([ASYNC_TOKEN, target]() {
        bool lan = false;
        std::string url = AppConfig::reachableUrl(target, &lan);
        brls::sync([ASYNC_TOKEN, url, lan]() {
            ASYNC_RELEASE
            if (url.empty()) {
                brls::Application::notify("main/network/unreachable"_i18n);
                return;
            }
            AppConfig::instance().setActiveUrl(url, lan);
            this->refresh();
            brls::Application::notify(this->applicable() && AppConfig::instance().isLanMode()
                                          ? "main/network/switched_lan"_i18n
                                          : "main/network/switched_wan"_i18n);
        });
    });
}

void NetworkBadge::forceSwitch() {
    auto& conf = AppConfig::instance();
    AppServer target = activeServer();
    if (target.id.empty()) return;

    // Flip to the other configured address; the probe is skipped on purpose — the
    // user is overriding its verdict.
    if (conf.isLanMode()) {
        if (target.urls.empty()) return;
        conf.setActiveUrl(target.urls.front(), false);
    } else {
        if (target.local_url.empty()) return;
        conf.setActiveUrl(target.local_url, true);
    }
    this->refresh();
}
