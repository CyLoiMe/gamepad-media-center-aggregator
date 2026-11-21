#include "utils/image.hpp"
#include "utils/thread.hpp"
#include <fmt/format.h>
#include <borealis/core/cache_helper.hpp>
#ifdef USE_WEBP
#include <webp/decode.h>
#endif
#include <stb_image.h>

Image::Image() : image(nullptr) {
    this->isCancel = std::make_shared<std::atomic_bool>(false);
    brls::Logger::verbose("new Image {}", fmt::ptr(this));
}

Image::~Image() { brls::Logger::verbose("delete Image {}", fmt::ptr(this)); }

void Image::with(brls::Image* view, const std::string& url, int width, int height) {
    // width/height only ever meant something to the old per-platform texture cap;
    // the artwork is uploaded at whatever size the server returned.
    (void)width;
    (void)height;

    int tex = brls::TextureCache::instance().getCache(url);
    if (tex > 0) {
        // The cache owns this texture. brls::Image defaults freeTexture to
        // true, so a view whose FIRST load is a cache hit (common on Stremio:
        // identical absolute URLs across row cells and detail pages) would
        // nvgDeleteImage a cached texture from clear()/its destructor and
        // leave a dead id in the cache — drawing it later faults on the GPU.
        view->setFreeTexture(false);
        view->innerSetImage(tex);
        return;
    }

    // One fresh Image per request. Recycling a pooled object whose previous
    // doRequest could still be in flight shared url/image/isCancel between two
    // requests: resetting isCancel here REVOKED the cancellation of the old
    // transfer, which then finished and cached its pixels under this request's
    // key (wrong artwork, persistent), while both sides raced on the fields.
    Ref item = std::make_shared<Image>();

    std::lock_guard<std::mutex> lock(requestMutex);

    auto it = requests.insert(std::make_pair(view, item));
    if (!it.second) {
        brls::Logger::warning("insert Image {} failed", fmt::ptr(view));
        return;
    }

    item->image = view;
    item->url = url;
    view->ptrLock();
    // 设置图片组件不处理纹理的销毁，由缓存统一管理纹理销毁
    view->setFreeTexture(false);

    ThreadPool::instance().submit([item](HTTP& s) { item->doRequest(s); });
}

void Image::cancel(brls::Image* view) {
    brls::TextureCache::instance().removeCache(view->getTexture());
    view->clear();

    clear(view);
}

void Image::doRequest(HTTP& s) {
    // clear() ends in view->ptrUnlock(), and ptrLockCounter is a plain int
    // only ever touched from the UI thread (Box::removeView, the free queue) —
    // so the worker-side failure paths must route it through brls::sync, like
    // the success path does.
    if (this->isCancel->load()) {
        auto* imagePtr = this->image.load();
        brls::sync([imagePtr] { Image::clear(imagePtr); });
        return;
    }
    try {
        std::ostringstream body;
        HTTP::set_option(s, this->isCancel, HTTP::Timeout{});
        s._get(this->url, &body);
        std::string data = body.str();
        uint8_t* imageData = nullptr;
        int imageW = 0, imageH = 0;
        bool isWebp = false;
#ifdef USE_WEBP
        // Prefer the RIFF....WEBP magic over Content-Type: it is the reliable
        // signal for a response that carries none, and survives a server that
        // labels the file as octet-stream.
        char* ct = nullptr;
        bool webpMagic = data.size() >= 12 && memcmp(data.data(), "RIFF", 4) == 0 &&
                         memcmp(data.data() + 8, "WEBP", 4) == 0;
        if (webpMagic || url.find("Webp") != std::string::npos ||
            (s.getinfo(&ct) && ct != nullptr && strcmp(ct, "image/webp") == 0)) {
            imageData = WebPDecodeRGBA((const uint8_t*)data.c_str(), data.size(), &imageW, &imageH);
            isWebp = true;
        } else
#endif
        {
            int n;
            imageData = stbi_load_from_memory((unsigned char*)data.c_str(), data.size(), &imageW, &imageH, &n, 4);
        }

        // exact GPU footprint of the upload, forwarded to the TextureCache
        // byte capacity; 0 = let addCache estimate (w*h*4)
        size_t texBytes = 0;
        auto* imagePtr = this->image.load();
        auto urlCopy = this->url;
        auto isCancelCopy = this->isCancel;
        int imageFlags = 0;

        brls::Logger::verbose("request Image {} size {}", urlCopy, data.size());
        brls::sync([imagePtr, urlCopy, isCancelCopy, imageData, imageW, imageH, isWebp, imageFlags, texBytes] {
            if (!isCancelCopy->load()) {
                // Load texture
                int tex = brls::TextureCache::instance().getCache(urlCopy);
                if (tex == 0 && imageData != nullptr) {
                    NVGcontext* vg = brls::Application::getNVGContext();
                    tex = nvgCreateImageRGBA(vg, imageW, imageH, imageFlags, imageData);
                    brls::TextureCache::instance().addCache(urlCopy, tex, texBytes);
                }
                if (tex > 0) imagePtr->innerSetImage(tex);
                clear(imagePtr);
            }
            if (imageData) {
#ifdef USE_WEBP
                if (isWebp)
                    WebPFree(imageData);
                else
#endif
                    stbi_image_free(imageData);
            }
        });
    } catch (const std::exception& ex) {
        brls::Logger::warning("request image {} {}", this->url, ex.what());
        auto* imagePtr = this->image.load();
        brls::sync([imagePtr] { Image::clear(imagePtr); });
    }
}

void Image::clear(brls::Image* view) {
    std::lock_guard<std::mutex> lock(requestMutex);

    auto it = requests.find(view);
    if (it == requests.end()) return;

    view->ptrUnlock();
    it->second->image = nullptr;
    it->second->isCancel->store(true);
    requests.erase(it);
}