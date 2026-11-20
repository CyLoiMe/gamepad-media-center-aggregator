#include "utils/forwarder.hpp"

#ifdef BUILTIN_NSP

#include <borealis.hpp>
#include <switch.h>
#include <nspmini.hpp>

#include "utils/config.hpp"  // AppVersion::nro_path

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace forwarder {

namespace {

/// The NSP this build packed into romfs: the only payload the app can install.
constexpr const char* kNspPath = "romfs:/forwarder.nsp";

/// ns hands back a fixed-size buffer; trailing zeros are padding, not artwork.
size_t contentLen(const unsigned char* p, size_t n) {
    while (n > 0 && p[n - 1] == 0) n--;
    return n;
}

std::string hex4(const unsigned char* p, size_t n) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02x%02x%02x%02x", n > 0 ? p[0] : 0, n > 1 ? p[1] : 0, n > 2 ? p[2] : 0,
        n > 3 ? p[3] : 0);
    return buf;
}

/// The icon this build carries: the ASET section of the NRO we were launched from
/// (`elf2nro --icon`, i.e. exactly what hbmenu shows for it). Empty when the path
/// is unknown or the section cannot be parsed — the caller then skips the check
/// rather than inventing a verdict.
std::vector<unsigned char> ownIcon() {
    const std::string& path = AppVersion::nro_path;
    if (path.empty()) return {};
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    const std::streamoff size = in.tellg();
    if (size <= 0) return {};
    // The ASET section sits at the very end (icon ~13 KB + NACP 16 KB); reading the
    // tail keeps this away from the 30 MB NRO body.
    const std::streamoff tail = std::min<std::streamoff>(size, 256 * 1024);
    std::vector<unsigned char> buf(static_cast<size_t>(tail));
    in.seekg(size - tail);
    if (!in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(tail))) return {};

    for (size_t i = 0; i + 40 < buf.size(); i++) {
        if (std::memcmp(&buf[i], "ASET", 4) != 0) continue;
        u64 iconOff = 0, iconSize = 0;
        std::memcpy(&iconOff, &buf[i + 8], sizeof iconOff);
        std::memcpy(&iconSize, &buf[i + 16], sizeof iconSize);
        if (iconSize == 0 || iconSize > 1024 * 1024) continue;      // no sane icon
        if (iconOff < 40 || i + iconOff + iconSize > buf.size()) continue;
        if (buf[i + iconOff] != 0xFF || buf[i + iconOff + 1] != 0xD8) continue;  // JPEG SOI
        return std::vector<unsigned char>(buf.begin() + i + iconOff, buf.begin() + i + iconOff + iconSize);
    }
    return {};
}

/// What the console has stored for our title, as icon bytes. `hasTitle` says
/// whether ns answered at all (a failed read is not an empty icon).
std::vector<unsigned char> installedIcon(bool& hasTitle) {
    hasTitle = false;
    if (R_FAILED(nsInitialize())) return {};
    auto* data = new NsApplicationControlData();
    u64 got = 0;
    const Result rc = nsGetApplicationControlData(
        NsApplicationControlSource_Storage, FORWARDER_TITLEID, data, sizeof(*data), &got);
    nsExit();

    std::vector<unsigned char> out;
    if (R_SUCCEEDED(rc)) {
        hasTitle = true;
        const size_t n = contentLen(data->icon, sizeof(data->icon));
        out.assign(data->icon, data->icon + n);
    }
    delete data;
    return out;
}

}  // namespace

Report install() {
    const std::vector<unsigned char> expected = ownIcon();

    if (!mini::InstallSD(kNspPath)) return {Outcome::Failed, "InstallSD refused the NSP"};

    bool hasTitle = false;
    const std::vector<unsigned char> actual = installedIcon(hasTitle);
    if (!hasTitle) return {Outcome::Installed, "icon check skipped: ns returned no control data"};
    if (expected.empty()) return {Outcome::Installed, "icon check skipped: own NRO icon unreadable"};

    const bool same = expected.size() == actual.size() &&
                      std::memcmp(expected.data(), actual.data(), actual.size()) == 0;
    char detail[160];
    std::snprintf(detail, sizeof detail, "this build %zu B %s / console %zu B %s", expected.size(),
        hex4(expected.data(), expected.size()).c_str(), actual.size(),
        hex4(actual.data(), actual.size()).c_str());
    return {same ? Outcome::Installed : Outcome::IconStale, detail};
}

}  // namespace forwarder

#else

namespace forwarder {
Report install() { return {Outcome::Failed, "built without -DBUILTIN_NSP=ON"}; }
}  // namespace forwarder

#endif
