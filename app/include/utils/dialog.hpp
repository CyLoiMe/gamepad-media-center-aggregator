#pragma once

#include <string>
#include <functional>

class Dialog {
public:
    using Callback = brls::VoidEvent::Callback;

    /// 展示普通对话框
    /// `cancelable`: borealis dismisses a dialog on B *without* running the
    /// button callback. Pass false when `cb` is the only way forward — a caller
    /// that clears a one-shot flag there would otherwise leave it stuck for the
    /// rest of the session after a stray B.
    static void show(const std::string& msg, Callback cb = []() {}, bool cancelable = true);

    /// 展示带有取消按钮的对话框
    static void cancelable(const std::string& msg, Callback cb);

    /// Quit-app prompt; `msg` tailors the text to the context
    /// (e.g. update installed), empty = generic message
    static void quitApp(bool restart = true, const std::string& msg = "");
};