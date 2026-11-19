// Standalone logic test — the WAN library allow-list rule.
//
// nullptr = never configured (nothing hidden, so an existing install keeps
// working), a list = authoritative (an empty one hides everything). See
// utils/wan_filter.hpp.

#include <cstdio>
#include <string>
#include <vector>

#include <utils/wan_filter.hpp>

static int failures = 0;
#define CHECK(cond)                                          \
    do {                                                     \
        if (!(cond)) {                                       \
            printf("FAIL: %s (line %d)\n", #cond, __LINE__);  \
            ++failures;                                      \
        }                                                    \
    } while (0)

int main() {
    const std::vector<std::string>* unset = nullptr;
    const std::vector<std::string> all{"115-movies", "115-shows", "nas-4k"};
    const std::vector<std::string> some{"115-movies", "115-shows"};
    const std::vector<std::string> none;

    // No allow-list configured: nothing is hidden.
    CHECK(wanfilter::allows(unset, "nas-4k"));
    CHECK(wanfilter::allows(unset, ""));
    CHECK(wanfilter::allows(unset, "anything"));

    // Configured list: membership decides.
    CHECK(wanfilter::allows(&some, "115-movies"));
    CHECK(wanfilter::allows(&some, "115-shows"));
    CHECK(!wanfilter::allows(&some, "nas-4k"));
    CHECK(!wanfilter::allows(&some, "some-new-library"));

    // Empty list is authoritative: everything hidden (a new library does not
    // become WAN-visible just because nobody looked at it).
    CHECK(!wanfilter::allows(&none, "115-movies"));
    CHECK(!wanfilter::allows(&none, ""));

    // An item that cannot name its library is hidden while filtering is active,
    // rather than risking a leak from an excluded library.
    CHECK(!wanfilter::allows(&some, ""));

    // "Allow everything" explicitly is the same as unset for every id.
    for (auto& id : all) CHECK(wanfilter::allows(&all, id));

    if (failures == 0) printf("test_wan_filter: OK\n");
    return failures == 0 ? 0 : 1;
}
