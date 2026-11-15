// Standalone logic test — reading the User-Agent out of the user's mpv.conf
// (utils/mpv_conf.hpp).
//
// The app sends ONE User-Agent (HTTP client + mpv): cloud-drive front-ends sign
// the URL they hand out with the UA of the request that asked for it, so a
// playback UA that differs is a CDN 403. This is the lane that lets a
// `user-agent=` line in mpv.conf feed that single value.
//
//   c++ -std=gnu++17 -arch x86_64 -Iapp/include tests/test_mpv_conf.cpp -o /tmp/t && /tmp/t

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utils/mpv_conf.hpp>

static int failures = 0;

static const std::string key = "user-agent";

/// `line` must yield exactly `want`.
static void expectValue(const char* line, const char* want) {
    auto v = mpvconf::lineValue(line, key);
    if (!v || *v != want) {
        ++failures;
        printf("FAIL: line <%s> -> %s (want <%s>)\n", line, v ? ("<" + *v + ">").c_str() : "none", want);
    }
}

/// `line` must not set the key at all.
static void expectNone(const char* line) {
    auto v = mpvconf::lineValue(line, key);
    if (v) {
        ++failures;
        printf("FAIL: line <%s> should set nothing (got <%s>)\n", line, v->c_str());
    }
}

int main() {
    // --- accepted spellings ------------------------------------------------
    expectValue("user-agent=Mozilla/5.0 (Windows NT 10.0; Win64; x64)", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    expectValue("--user-agent=Mozilla/5.0", "Mozilla/5.0");  // leading --, as in the CLI docs
    expectValue("   user-agent=Mozilla", "Mozilla");         // indented
    expectValue("\tuser-agent=Mozilla\r", "Mozilla");        // CRLF config file
    expectValue("user-agent = Mozilla", "Mozilla");          // blanks around '='
    expectValue("user-agent=Mozilla  # note", "Mozilla");    // trailing comment dropped
    expectValue("user-agent=\"GMCA/1.4.0 (NX)\"", "GMCA/1.4.0 (NX)");  // quotes stripped
    expectValue("user-agent=", "");                          // set to empty on purpose

    // --- rejected spellings ------------------------------------------------
    expectNone("volume=50");                            // another option
    expectNone("#user-agent=Mozilla");                  // commented out
    expectNone("  # user-agent=Mozilla");
    expectNone("user_agent=Mozilla");                   // '_' is not a separator in mpv
    expectNone("user-agent-fast=Mozilla");              // other option
    expectNone("x-user-agent=Mozilla");
    expectNone("user-agent");                           // a bare name is a flag toggle
    expectNone("");
    expectNone("   ");
    expectNone("=");
    expectNone("http-header-fields=User-Agent: Mozilla");  // the name appears in a value

    // --- whole-file scan ---------------------------------------------------
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "gmca_test_mpv_conf";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const std::string base = dir.string();

    if (mpvconf::value(base, key)) {  // no mpv.conf at all
        ++failures;
        printf("FAIL: missing mpv.conf should yield nothing\n");
    }
    if (mpvconf::value(base + "/nope", key)) {  // no such directory
        ++failures;
        printf("FAIL: missing directory should yield nothing\n");
    }

    std::ofstream(base + "/mpv.conf") << "# mine\nvolume=50\nhwdec=auto-safe\n";
    if (mpvconf::value(base, key)) {
        ++failures;
        printf("FAIL: a file without the key should yield nothing\n");
    }

    std::ofstream(base + "/mpv.conf") << "volume=50\nuser-agent=GMCA/1.4.0 (NX)\nuser-agent=second\n";
    auto v = mpvconf::value(base, key);
    if (!v || *v != "GMCA/1.4.0 (NX)") {  // first occurrence wins, like mpv
        ++failures;
        printf("FAIL: file scan -> %s\n", v ? v->c_str() : "none");
    }

    fs::remove_all(dir, ec);

    if (failures == 0)
        printf("test_mpv_conf: all checks passed\n");
    else
        printf("test_mpv_conf: %d FAILED\n", failures);
    return failures ? 1 : 0;
}
