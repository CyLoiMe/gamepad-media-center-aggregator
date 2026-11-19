// Standalone logic test — what an in-place connection edit must preserve.
//
// The edit form rebuilds the profile from the login response, which carries
// neither the WAN allow-list nor the sidebar layout: both are keyed by the ids a
// re-login can change. See utils/connection_edit.hpp.

#include <cstdio>
#include <string>
#include <vector>

#include <utils/connection_edit.hpp>

static int failures = 0;
#define CHECK(cond)                                          \
    do {                                                     \
        if (!(cond)) {                                       \
            printf("FAIL: %s (line %d)\n", #cond, __LINE__);  \
            ++failures;                                      \
        }                                                    \
    } while (0)

/// Stand-in for AppUser: `hasLibraries` = whether the allow-list is engaged.
/// An explicit constructor: a default member initialiser is not an aggregate on
/// older compilers, and tests/run.sh must work on whatever C++17 compiler exists.
struct Profile {
    std::string server_id;
    bool hasLibraries;
    std::vector<std::string> libraries;

    explicit Profile(const std::string& id, bool has = false, const std::vector<std::string>& libs = {})
        : server_id(id), hasLibraries(has), libraries(libs) {}
};

int main() {
    // --- the WAN allow-list survives an edit that does not mention it ----------
    Profile stored{"srv-1", true, {"movies"}};
    Profile edited{"srv-1"};
    connection_edit::keepProfileSettings(edited, stored);
    CHECK(edited.hasLibraries);
    CHECK(edited.libraries.size() == 1);
    CHECK(edited.libraries.at(0) == "movies");

    // A list the edit DID set (a future settings screen) is not overwritten.
    Profile explicitEdit{"srv-1", true, {"shows"}};
    connection_edit::keepProfileSettings(explicitEdit, stored);
    CHECK(explicitEdit.libraries.at(0) == "shows");

    // "Never configured" stays "never configured": nothing is invented.
    Profile never{"srv-1"};
    Profile alsoNever{"srv-1"};
    connection_edit::keepProfileSettings(alsoNever, never);
    CHECK(!alsoNever.hasLibraries);

    // --- the old server entry is dropped only when it changed and is unused ----
    // Same id: rewritten in place, never dropped.
    CHECK(!connection_edit::dropOldEntry("srv-1", "srv-1", false));
    // New id, nothing else points at it: the stale entry goes.
    CHECK(connection_edit::dropOldEntry("srv-1", "srv-2", false));
    // New id but another profile still lives on it: keep it.
    CHECK(!connection_edit::dropOldEntry("srv-1", "srv-2", true));
    // Not stored yet (adding): nothing to drop.
    CHECK(!connection_edit::dropOldEntry("", "srv-2", false));

    // --- reference check: the edited profile points at the NEW id --------------
    std::vector<Profile> profiles;
    profiles.push_back(Profile{"srv-2"});  // the edited one
    CHECK(!connection_edit::serverStillReferenced(profiles, "srv-1"));
    profiles.push_back(Profile{"srv-1"});  // another account
    CHECK(connection_edit::serverStillReferenced(profiles, "srv-1"));
    CHECK(!connection_edit::serverStillReferenced(profiles, ""));

    if (failures == 0) printf("test_connection_edit: OK\n");
    return failures == 0 ? 0 : 1;
}
