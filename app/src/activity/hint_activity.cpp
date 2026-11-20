#include "activity/hint_activity.hpp"

#ifdef BUILTIN_NSP
#include <switch.h>
#include "utils/forwarder.hpp"
#endif

using namespace brls::literals;

HintActivity::HintActivity() { brls::Logger::debug("HintActivity: create"); }

void HintActivity::onContentAvailable() {
    brls::Logger::debug("HintActivity: onContentAvailable");

    // The same screen serves both contexts: at applet boot (insufficient
    // memory) and from settings / first launch in application mode
    // (HOME tile installation). Only the text changes.
    if (brls::Application::getPlatform()->isApplicationMode()) {
        this->labelText->setText("main/hints/text_app"_i18n);
    }

#ifdef BUILTIN_NSP
    this->btnInstall->registerClickAction([](...) -> bool {
        auto dialog = new brls::Dialog("main/hints/hint_confirm"_i18n);
        dialog->addButton("hints/cancel"_i18n, []() {});
        dialog->addButton("hints/ok"_i18n, []() {
            brls::Application::blockInputs();
            // The install is verified, not assumed: forwarder::install() writes the
            // embedded NSP, then reads the installed title's control data back and
            // compares that icon with the one this build carries. "Done" is only
            // shown when the console really kept our artwork — a stale icon used to
            // look exactly like a successful install.
            const forwarder::Report report = forwarder::install();
            brls::Application::unblockInputs();
            brls::Logger::info("forwarder: outcome {} — {}", static_cast<int>(report.outcome), report.detail);

            if (report.outcome == forwarder::Outcome::Failed) {
                brls::Logger::error("forwarder: the NSP was not written ({})", report.detail);
                auto failed = new brls::Dialog("main/hints/install_failed"_i18n);
                failed->addButton("hints/ok"_i18n, []() {});
                failed->open();
                return;
            }

            // One title id, one installed tile: launch what we just wrote rather
            // than asking nspmini which title it thinks it installed.
            const u64 titleId = FORWARDER_TITLEID;
            if (report.outcome == forwarder::Outcome::IconStale) {
                // Written, but the console answers with different artwork. Nothing
                // else in the app can change that; say so instead of launching a
                // tile the user will find wrong again.
                brls::Logger::warning("forwarder: the console kept other artwork ({})", report.detail);
                auto stale = new brls::Dialog("main/hints/icon_stale"_i18n);
                stale->addButton("hints/ok"_i18n, [titleId]() { appletRequestLaunchApplication(titleId, NULL); });
                stale->open();
                return;
            }

            // The old pleNx HOME tile is a separate installed title that stays
            // until removed by hand (updating the app can't touch it). Point the
            // user at it, then launch the freshly installed SMCA tile.
            auto info = new brls::Dialog("main/hints/remove_old"_i18n);
            info->addButton("hints/ok"_i18n, [titleId]() { appletRequestLaunchApplication(titleId, NULL); });
            info->open();
        });
        dialog->open();
        return true;
    });
    this->btnInstall->addGestureRecognizer(new brls::TapGestureRecognizer(this->btnInstall));
#else
    // no embedded NSP in this build: only the title takeover option remains
    this->boxInstall->setVisibility(brls::Visibility::GONE);
#endif
}

HintActivity::~HintActivity() { brls::Logger::debug("HintActivity: delete"); }
