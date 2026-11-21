#pragma once

#include <atomic>
#include <borealis.hpp>
#include "api/http.hpp"
#include "api/backend.hpp"
#include "config.hpp"
#include "image_cache.hpp"

class Image {
    using Ref = std::shared_ptr<Image>;

public:
    Image();
    Image(const Image&) = delete;

    virtual ~Image();

    /// Loads an image for a backend path: the on-disk cached asset if present
    /// (offline), else the active backend's image URL. width/height > 0 requests
    /// backend-side resize where the backend supports it.
    static void load(brls::Image* view, const std::string& path, int width = 0, int height = 0) {
        if (path.empty()) return;
        // offline cache wins: a locally cached asset renders without the server
        // and gives downloaded content instant local artwork even online
        // (SPEC §4.2, AC6/AC17). Keyed by the raw path/url passed here.
        if (ImageCache::has(path)) {
            // borealis reads the cached file itself; the artwork keeps whatever
            // size it was cached at
            view->setImageFromFile(ImageCache::localPath(path));
            return;
        }
        // backend-specific URL building (Plex /photo/:/transcode, Jellyfin /Images...);
        // absolute external paths (cast faces...) are returned unchanged by the backend
        std::string url = AppConfig::instance().backend().imageUrl(path, width, height);
        // width/height reach the BACKEND only (server-side resize); a backend that
        // cannot resize — Stremio's absolute Cinemeta/RPDB urls — hands the
        // artwork over at its own size, and it is uploaded as-is.
        if (!url.empty()) with(view, url, width, height);
    }

    /// @brief 设置要加载内容的图片组件。此函数需要工作在主线程。
    /// width/height are forwarded to the backend's imageUrl() only.
    static void with(brls::Image* view, const std::string& url, int width = 0, int height = 0);

    /// @brief 取消请求，并清空图片。此函数需要工作在主线程。
    static void cancel(brls::Image* view);

private:
    void doRequest(HTTP& s);

    static void clear(brls::Image* view);

private:
    std::string url;
    // written by clear() (UI thread) while doRequest (worker) reads it on its
    // cancel/error paths — atomic so neither side sees a torn pointer
    std::atomic<brls::Image*> image;
    HTTP::Cancel isCancel;

    inline static std::mutex requestMutex;
    inline static std::unordered_map<brls::Image*, Ref> requests;
};