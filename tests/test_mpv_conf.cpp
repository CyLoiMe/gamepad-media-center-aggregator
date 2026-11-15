// Standalone logic test — user mpv.conf scanning (utils/mpv_conf.hpp).
//
// The app must stay silent about an option the user's mpv.conf already sets,
// otherwise mpv's own precedence (set_option_string == command line, which beats
// the config file) would silently discard it.
//
//   c++ -std=gnu++17 -arch x86_64 -Iapp/include tests/test_mpv_conf.cpp -o /tmp/t && /tmp/t

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utils/mpv_conf.hpp>

static int failures = 0;
#define CHECK(cond)                                          \
    do {                                                     \
        if (!(cond)) {                                       \
            printf("FAIL: %s (line %d)\n", #cond, __LINE__); \
            ++failures;                                      \
        }                                                    \
    } while (0)

int main() {
    const std::string key = "user-agent";

    // --- accepted spellings ------------------------------------------------
    CHECK(mpvconf::lineSets("user-agent=Mozilla/5.0 (Windows NT 10.0; Win64; x64)", key));
    CHECK(mpvconf::lineSets("--user-agent=Mozilla/5.0", key));  // leading -- as in the CLI docs
    CHECK(mpvconf::lineSets("   user-agent=Mozilla", key));     // indented
    CHECK(mpvconf::lineSets("\tuser-agent=Mozilla\r", key));    // CRLF config file
    CHECK(mpvconf::lineSets("user-agent = Mozilla", key));      // blanks around '='
    CHECK(mpvconf::lineSets("user-agent=", key));               // set to empty on purpose
    CHECK(mpvconf::lineSets("user-agent=Mozilla  # note", key));  // a trailing comment changes nothing
    CHECK(mpvconf::lineSets("volume=50", key) == false);        // sanity: another option

    // --- rejected spellings ------------------------------------------------
    CHECK(mpvconf::lineSets("#user-agent=Mozilla", key) == false);         // commented out
    CHECK(mpvconf::lineSets("  # user-agent=Mozilla", key) == false);
    CHECK(mpvconf::lineSets("user_agent=Mozilla", key) == false);          // '_' is not a separator in mpv
    CHECK(mpvconf::lineSets("user-agent-fast=Mozilla", key) == false);     // prefix of another option
    CHECK(mpvconf::lineSets("x-user-agent=Mozilla", key) == false);
    CHECK(mpvconf::lineSets("user-agent", key) == false);                  // a bare name is a flag toggle
    CHECK(mpvconf::lineSets("", key) == false);
    CHECK(mpvconf::lineSets("   ", key) == false);
    CHECK(mpvconf::lineSets("=", key) == false);
    CHECK(mpvconf::lineSets("http-header-fields=User-Agent: Mozilla", key) == false);  // name, not a value

    // --- file scan, including the missing-file / missing-dir cases ---------
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "gmca_test_mpv_conf";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    const std::string base = dir.string();

    CHECK(mpvconf::sets(base, key) == false);  // no mpv.conf at all
    CHECK(mpvconf::sets(base + "/nope", key) == false);  // no such directory

    std::ofstream(base + "/mpv.conf") << "# my settings\nvolume=50\nhwdec=auto-safe\n";
    CHECK(mpvconf::sets(base, key) == false);

    std::ofstream(base + "/mpv.conf") << "volume=50\nuser-agent=Mozilla/5.0\n";
    CHECK(mpvconf::sets(base, key));

    fs::remove_all(dir, ec);

    if (failures == 0)
        printf("test_mpv_conf: all checks passed\n");
    else
        printf("test_mpv_conf: %d FAILED\n", failures);
    return failures ? 1 : 0;
}
