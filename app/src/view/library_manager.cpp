/*
    SMCA — libraries manager (see library_manager.hpp).
*/

#include "view/library_manager.hpp"
#include "view/svg_image.hpp"
#include "utils/config.hpp"
#include "utils/network_state.hpp"
#include <algorithm>
#include <optional>

using namespace brls::literals;  // for _i18n

namespace {

/// linear RGBA blend (t=0 -> a, t=1 -> b).
NVGcolor mix(NVGcolor a, NVGcolor b, float t) {
    NVGcolor c;
    for (int i = 0; i < 4; i++) c.rgba[i] = a.rgba[i] * (1.f - t) + b.rgba[i] * t;
    return c;
}

/// shell of the manager screen: title + description + a scrollable, centered
/// column of rows (populated programmatically).
const std::string managerXML = R"xml(
    <brls:Box
        axis="column"
        grow="1"
        justifyContent="flexStart"
        alignItems="center"
        paddingTop="40"
        paddingLeft="30"
        paddingRight="30"
        paddingBottom="20">

        <brls:Label
            fontSize="30"
            text="@i18n/main/setting/libraries/header"
            marginBottom="6" />

        <brls:Label
            id="lib_manager/desc"
            fontSize="15"
            horizontalAlign="center"
            text="@i18n/main/setting/libraries/description"
            marginBottom="24" />

        <brls:ScrollingFrame
            width="100%"
            grow="1">
            <brls:Box
                width="100%"
                axis="column"
                alignItems="center"
                paddingTop="4"
                paddingBottom="20">
                <brls:Box
                    id="lib_manager/rows"
                    width="680"
                    axis="column" />
            </brls:Box>
        </brls:ScrollingFrame>

    </brls:Box>
)xml";

/// One reorderable row: icon + name + a visible/hidden tag. Fully custom focus
/// / grab visuals (the Borealis highlight is hidden), mirroring ConnectionTile.
class LibraryRow : public brls::Box {
public:
    LibraryRow(LibraryManager* mgr, int index, const MainTabFrame::SidebarEntry& e)
        : mgr(mgr), index(index), shown(e.visible) {
        auto theme = brls::Application::getTheme();
        this->accent = theme.getColor("color/app");
        this->surface = theme.getColor("color/surface");
        this->text = theme.getColor("brls/text");
        this->grey = theme.getColor("font/grey");

        this->setAxis(brls::Axis::ROW);
        this->setWidthPercentage(100);
        this->setHeight(60);
        this->setCornerRadius(12);
        this->setMarginBottom(8);
        this->setPaddingLeft(18);
        this->setPaddingRight(18);
        this->setAlignItems(brls::AlignItems::CENTER);
        this->setFocusable(true);
        this->setHideHighlight(true);

        this->icon = new SVGImage();
        this->icon->setWidth(24);
        this->icon->setHeight(24);
        this->icon->setMarginRight(16);
        std::string path = e.icon;
        const std::string prefix = "@res/";
        if (path.rfind(prefix, 0) == 0) path = path.substr(prefix.size());
        this->icon->setImageFromSVGRes(path);
        this->addView(this->icon);

        this->label = new brls::Label();
        this->label->setText(e.label);
        this->label->setFontSize(18);
        this->label->setGrow(1.0f);
        this->addView(this->label);

        // WAN column (Jellyfin/Emby only): a library switched off here is hidden
        // from the app whenever the connection is on its tunnel address.
        this->wanApplicable = mgr->wanCapable() && e.library;
        this->wanOn = this->wanApplicable && mgr->wanShown(e.id);
        if (this->wanApplicable) {
            this->wanTag = new brls::Label();
            this->wanTag->setText("main/setting/libraries/wan"_i18n);
            this->wanTag->setFontSize(14);
            this->wanTag->setMarginRight(18);
            this->addView(this->wanTag);

            // X: allow / hide on WAN (ignored while a row is grabbed, like Y)
            this->registerAction(
                "main/setting/libraries/wan_toggle"_i18n, brls::BUTTON_X, [mgr, index](brls::View*) {
                    if (mgr->anyGrabbed()) return true;
                    mgr->toggleWan(index);
                    return true;
                });
        }

        this->tag = new brls::Label();
        this->tag->setText(this->shown ? "main/setting/libraries/visible"_i18n
                                        : "main/setting/libraries/hidden"_i18n);
        this->tag->setFontSize(14);
        this->addView(this->tag);

        bool grabbed = mgr->isGrabbed(index);

        // A: grab / drop
        this->registerAction(
            grabbed ? "main/setting/libraries/drop"_i18n : "main/setting/libraries/grab"_i18n, brls::BUTTON_A,
            [mgr, index](brls::View*) {
                mgr->toggleGrab(index);
                return true;
            });

        // Y: show / hide (ignored while a row is grabbed)
        this->registerAction("main/setting/libraries/toggle"_i18n, brls::BUTTON_Y, [mgr, index](brls::View*) {
            if (mgr->anyGrabbed()) return true;  // swallow: no visibility change mid-move
            mgr->toggleVisible(index);
            return true;
        });

        // D-pad up/down: move while grabbed (consumes navigation), otherwise
        // let the focus navigate normally (return false)
        this->registerAction(
            "", brls::BUTTON_NAV_UP,
            [mgr, index](brls::View*) {
                if (!mgr->isGrabbed(index)) return false;
                mgr->moveGrabbed(-1);
                return true;
            },
            true, true);
        this->registerAction(
            "", brls::BUTTON_NAV_DOWN,
            [mgr, index](brls::View*) {
                if (!mgr->isGrabbed(index)) return false;
                mgr->moveGrabbed(1);
                return true;
            },
            true, true);

        // mouse / touch: a tap toggles visibility (Y is gamepad-only, so this
        // is the show/hide affordance for pointer input). Ignored mid-grab.
        this->addGestureRecognizer(new brls::TapGestureRecognizer(this, [mgr, index]() {
            if (mgr->anyGrabbed()) return;
            mgr->toggleVisible(index);
        }));

        this->applyVisual(false);
    }

    void onFocusGained() override {
        brls::Box::onFocusGained();
        this->applyVisual(true);
    }
    void onFocusLost() override {
        brls::Box::onFocusLost();
        this->applyVisual(false);
    }

private:
    void applyVisual(bool focused) {
        bool grabbed = this->mgr->isGrabbed(this->index);
        bool lift = focused || grabbed;

        float tint = grabbed ? 0.30f : (focused ? 0.18f : 0.f);
        this->setBackgroundColor(tint > 0 ? mix(this->surface, this->accent, tint) : nvgRGBA(0, 0, 0, 0));
        this->setBorderColor(this->accent);
        this->setBorderThickness(grabbed ? 3.f : (focused ? 2.f : 0.f));
        this->setShadowType(lift ? brls::ShadowType::GENERIC : brls::ShadowType::NONE);
        this->setShadowVisibility(lift);

        // dim hidden rows at rest so the state reads at a glance; keep them
        // fully legible while focused/grabbed
        this->setAlpha((!this->shown && !lift) ? 0.45f : 1.f);

        this->label->setTextColor(lift ? this->accent : this->text);
        this->tag->setTextColor(this->shown ? this->accent : this->grey);
        if (this->wanTag) this->wanTag->setTextColor(this->wanOn ? this->accent : this->grey);
    }

    LibraryManager* mgr;
    int index;
    bool shown;
    bool wanApplicable = false;
    bool wanOn = false;

    NVGcolor accent {}, surface {}, text {}, grey {};
    SVGImage* icon = nullptr;
    brls::Label* label = nullptr;
    brls::Label* tag = nullptr;
    brls::Label* wanTag = nullptr;
};

}  // namespace

LibraryManager::LibraryManager(MainTabFrame* frame) : frame(frame) {
    this->inflateFromXMLString(managerXML);
    this->entries = frame->getReorderableEntries();

    // The WAN column only exists where something enforces it: the Jellyfin/Emby
    // backend filters and the playback guard checks. Without a connection there
    // is no profile to write the list to either.
    const std::string type = AppConfig::instance().activeServerType();
    this->wanEnabled = (type == "jellyfin" || type == "emby") && !AppConfig::instance().getUserId().empty();
    if (this->wanEnabled) this->wanLibs = AppConfig::instance().getWanVisibleLibraries();

    this->rowsBox = dynamic_cast<brls::Box*>(this->getView("lib_manager/rows"));
    if (auto* desc = dynamic_cast<brls::Label*>(this->getView("lib_manager/desc"))) {
        desc->setTextColor(brls::Application::getTheme().getColor("font/grey"));
        std::string text = "main/setting/libraries/description"_i18n;
        if (this->wanEnabled) text += "\n" + "main/setting/libraries/wan_hint"_i18n;
        if (NetworkState::isOffline()) text += "\n" + "main/setting/libraries/offline"_i18n;
        desc->setText(text);
    }

    this->rebuild();
    this->live = true;
    brls::Logger::debug("LibraryManager: create ({} entries)", this->entries.size());
}

void LibraryManager::rebuild() {
    if (!this->rowsBox) return;
    this->rowsBox->clearViews();
    this->focusTarget = nullptr;

    if (this->entries.empty()) {
        auto* empty = new brls::Label();
        empty->setText("main/setting/libraries/empty"_i18n);
        empty->setFontSize(16);
        empty->setTextColor(brls::Application::getTheme().getColor("font/grey"));
        empty->setMarginTop(40);
        this->rowsBox->addView(empty);
        return;
    }

    for (int i = 0; i < (int)this->entries.size(); i++)
        this->rowsBox->addView(new LibraryRow(this, i, this->entries[i]));

    int idx = this->pendingFocus;
    if (idx < 0) idx = 0;
    if (idx >= (int)this->entries.size()) idx = (int)this->entries.size() - 1;
    this->focusTarget = this->rowsBox->getChildren()[idx];

    if (this->live && this->focusTarget) brls::Application::giveFocus(this->focusTarget);
}

void LibraryManager::commit() {
    std::vector<std::string> order;
    std::set<std::string> hidden;
    order.reserve(this->entries.size());
    for (auto& e : this->entries) {
        order.push_back(e.id);
        if (!e.visible) hidden.insert(e.id);
    }
    this->frame->setSidebarLayout(order, hidden);
}

void LibraryManager::toggleGrab(int index) {
    this->grabbedIndex = (this->grabbedIndex == index) ? -1 : index;
    this->pendingFocus = index;
    this->rebuild();
}

void LibraryManager::moveGrabbed(int delta) {
    if (this->grabbedIndex < 0) return;
    int j = this->grabbedIndex + delta;
    if (j < 0 || j >= (int)this->entries.size()) return;

    std::swap(this->entries[this->grabbedIndex], this->entries[j]);
    this->grabbedIndex = j;
    this->pendingFocus = j;

    this->commit();   // persist + reflect in the sidebar live
    this->rebuild();  // rebuild the manager rows + refocus the moved row
}

void LibraryManager::toggleVisible(int index) {
    if (index < 0 || index >= (int)this->entries.size()) return;
    this->entries[index].visible = !this->entries[index].visible;
    this->pendingFocus = index;

    this->commit();
    this->rebuild();
}

bool LibraryManager::wanShown(const std::string& id) const {
    if (!this->wanLibs) return true;  // never configured: everything visible
    const std::string key = id.rfind("lib/", 0) == 0 ? id.substr(4) : id;
    return std::find(this->wanLibs->begin(), this->wanLibs->end(), key) != this->wanLibs->end();
}

void LibraryManager::toggleWan(int index) {
    if (!this->wanEnabled) return;
    if (index < 0 || index >= (int)this->entries.size()) return;
    if (!this->entries[index].library) return;
    const std::string key = this->entries[index].id.substr(4);

    // Materialise the list first: switching one library off while nothing is
    // configured must state the others explicitly, otherwise "not configured"
    // would keep meaning "all visible".
    std::vector<std::string> libs;
    if (this->wanLibs) {
        libs = *this->wanLibs;
    } else {
        for (auto& e : this->entries)
            if (e.library) libs.push_back(e.id.substr(4));
    }

    auto it = std::find(libs.begin(), libs.end(), key);
    if (it == libs.end())
        libs.push_back(key);
    else
        libs.erase(it);

    // Everything allowed again == nothing configured: store nullopt so a library
    // added to the server later is visible on WAN without a second visit.
    const bool all = std::all_of(this->entries.begin(), this->entries.end(), [&libs](const MainTabFrame::SidebarEntry& e) {
        return !e.library ||
               std::find(libs.begin(), libs.end(), e.id.substr(4)) != libs.end();
    });
    if (all)
        this->wanLibs = std::nullopt;
    else
        this->wanLibs = libs;
    AppConfig::instance().setWanVisibleLibraries(this->wanLibs);

    this->pendingFocus = index;
    this->rebuild();
}

brls::View* LibraryManager::getDefaultFocus() {
    return this->focusTarget ? this->focusTarget : brls::Box::getDefaultFocus();
}
