#include <borealis.hpp>
#include "utils/image_cache.hpp"
#include "utils/config.hpp"
#include "utils/misc.hpp"
#include "utils/network_state.hpp"
#include "api/http.hpp"
#include "api/backend.hpp"

#include <mutex>
#include <unordered_set>
#include <vector>

namespace ImageCache {

// In-memory index of cached keys so has() never touches the disk on the UI
// thread (it is called from every Image::load). Populated at init(), kept in
// sync by store()/remove().
static std::mutex g_mutex;
static std::unordered_set<std::string> g_keys;

namespace {

/// A relative path is the server's own id; an absolute URL brings its own host.
bool isExternal(const std::string& pathOrUrl) { return pathOrUrl.rfind("http", 0) == 0; }

/// Key input: the connection is part of it (see the header comment). 0x1f cannot
/// occur in either side, so two different (server, path) pairs cannot collide.
std::string scopedInput(const std::string& pathOrUrl) {
    if (isExternal(pathOrUrl)) return pathOrUrl;
    return AppConfig::instance().activeServerId() + "\x1f" + pathOrUrl;
}

/// Every entry that could belong to `pathOrUrl`: the active connection's, the
/// ones other servers stored, and the bare key written before the key carried a
/// connection at all.
std::vector<std::string> allKeys(const std::string& pathOrUrl) {
    std::vector<std::string> keys{key(scopedInput(pathOrUrl))};
    if (isExternal(pathOrUrl)) return keys;
    for (auto& s : AppConfig::instance().getServers()) keys.push_back(key(s.id + "\x1f" + pathOrUrl));
    keys.push_back(key(pathOrUrl));
    return keys;
}

/// Hash of the entry that may answer for `pathOrUrl`, "" when there is none.
/// The active connection first — that is the only one trusted online, where a
/// miss just re-fetches. Offline nothing can be re-fetched, so a copy cached
/// under another connection, or under none, is better than a blank poster: a
/// downloaded item must keep its artwork whatever happened to be active before
/// the network went away.
std::string findCached(const std::string& pathOrUrl) {
    const std::string own = key(scopedInput(pathOrUrl));
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_keys.count(own)) return own;
    }
    if (isExternal(pathOrUrl) || !NetworkState::isOffline()) return "";
    for (const auto& h : allKeys(pathOrUrl)) {
        if (h == own) continue;
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_keys.count(h)) return h;
    }
    return "";
}

}  // namespace

std::string dir() { return AppConfig::instance().configDir() + "/downloads/art"; }

std::string localPath(const std::string& pathOrUrl) {
    // The entry that would answer (possibly another server's, offline), else the
    // name store() would write.
    const std::string found = findCached(pathOrUrl);
    return dir() + "/" + (found.empty() ? key(scopedInput(pathOrUrl)) : found);
}

void init() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_keys.clear();
    try {
        std::string d = dir();
        if (!fs::exists(d)) return;
        for (auto& e : fs::directory_iterator(d)) {
            std::string name = e.path().filename().string();
            // keys are 16 hex chars, no extension; skip .part temporaries
            if (name.find('.') == std::string::npos) g_keys.insert(name);
        }
    } catch (const std::exception& ex) {
        brls::Logger::warning("ImageCache: init scan failed: {}", ex.what());
    }
}

bool has(const std::string& pathOrUrl) {
    if (pathOrUrl.empty()) return false;
    return !findCached(pathOrUrl).empty();
}

bool store(const std::string& pathOrUrl) {
    if (pathOrUrl.empty()) return false;

    const std::string hash = key(scopedInput(pathOrUrl));
    const std::string local = dir() + "/" + hash;
    try {
        if (fs::exists(local)) {  // already cached
            std::lock_guard<std::mutex> lock(g_mutex);
            g_keys.insert(hash);
            return true;
        }
    } catch (...) {
    }

    // relative server path -> original file on the server (no resize, one file
    // per path whatever the requested display size). Absolute URL stays as-is.
    std::string url =
        pathOrUrl.rfind("http", 0) == 0 ? pathOrUrl : AppConfig::instance().backend().imageUrl(pathOrUrl);

    try {
        std::string d = dir();
        if (!fs::exists(d)) fs::create_directories(d);
        // write to a temp then rename so a half-written file never reads as
        // present (has() must only be true for complete assets)
        std::string tmp = local + ".part";
        HTTP::download(url, tmp, HTTP::Timeout{});
        fs::rename(tmp, local);
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_keys.insert(hash);
        }
        return true;
    } catch (const std::exception& e) {
        brls::Logger::warning("ImageCache: store failed {}: {}", pathOrUrl, e.what());
        return false;
    }
}

void remove(const std::string& pathOrUrl) {
    if (pathOrUrl.empty()) return;
    // every connection's copy: the caller is pruning a downloaded item, and a
    // leftover file under another server's key would outlive it
    for (const auto& hash : allKeys(pathOrUrl)) {
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_keys.erase(hash);
        }
        try {
            std::string f = dir() + "/" + hash;
            if (fs::exists(f)) fs::remove(f);
        } catch (const std::exception& e) {
            brls::Logger::warning("ImageCache: remove failed {}: {}", pathOrUrl, e.what());
        }
    }
}

}  // namespace ImageCache
