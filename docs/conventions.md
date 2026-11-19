# Conventions

Inherited from GMCA and kept on purpose: the code is consistent and a diff stays
reviewable.

## Code

- **C++17**, 4 spaces, 120 columns, `.clang-format` (Google base, `PointerAlignment:
  Left`, `SortIncludes: Never`). Run `clang-format -i` on the files you touch.
- **English** in identifiers, comments and commit messages. Existing comments in
  Chinese/French are history, not a style to copy.
- **Naming**: `snake_case()` functions and variables, `PascalCase` types,
  `kConstant`-free all-caps for macros/constants (`INMEMORY_CACHE`), members
  without a prefix (`this->` when it disambiguates).
- **File header comments** explain *why*, not *what*. A non-obvious workaround gets
  a comment with the reason and, where possible, the observation behind it (a log
  line, a version, an issue).
- **No exceptions across threads.** `brls::async` work reports through the
  `then`/`error` callbacks; `resolvePlayback` is the one synchronous verb and it
  throws (the caller catches and shows a dialog).
- **Ownership**: RAII everywhere, `std::unique_ptr` for owned polymorphic objects
  (`AppConfig::activeBackend`), no bare `new`/`delete` outside the borealis view
  tree (which owns its children).
- **No new platform branches.** The project is Switch-only; `#ifdef __PSV__`-style
  leftovers from upstream are dead code, do not add more.
- **Pure logic goes into a header** under `app/include/` (no borealis, no mpv) so
  `tests/run.sh` can compile and run it. `utils/mpv_conf.hpp` and
  `api/jellyfin/playback_info.hpp` are the model.

## Settings

Adding one setting touches four places, all of them:

1. `AppConfig::Item` (`app/include/utils/config.hpp`) — the enum entry;
2. `AppConfig::settingMap` (`app/src/utils/config.cpp`) — the JSON key, plus the
   option/value lists when it is a `SelectorCell`;
3. `AppConfig::init()` — load it into whatever static the runtime reads
   (`MPVCore::*`, `HTTP::*`, …);
4. `resources/xml/tabs/settings.xml` + a `BRLS_BIND` and `init()` in
   `tab/setting_tab.cpp`, and the i18n key.

The settings **cell writes**: `setItem()` (persists `config.json`) *and* updates the
runtime value, so the change is live without a restart (the player is restarted when
an mpv-level option changes, and only if it already exists — `MPVCore::created()`).

## i18n

- Every user-visible string lives in `resources/i18n/<locale>/main.json` (or
  `hints.json`), **all 14 locales** — a missing key shows the raw key in that
  language.
- In C++: `"main/setting/..."_i18n`; in XML: `@i18n/main/...`.
- Keep the JSON formatting as-is (2 spaces); do not reorder or reformat a whole
  file for one key.

## Tests

- Standalone: `tests/test_*.cpp` with `main()`, `-Iapp/include`, no CMake wiring,
  run by `tests/run.sh`.
- Add one when the logic is pure (parsing, selection, mapping, redaction). Do not
  try to test anything that needs borealis, mpv or a live server.

## Commits and branches

- **Conventional Commits**, English, scope = area: `feat(player)`, `fix(jellyfin)`,
  `fix(http)`, `refactor`, `docs`, `build`, `test`, `chore`. Breaking changes use
  `!`.
- One logical change per commit; the body explains the *why* and the evidence, not
  the diff.
- **No AI attribution** (no "Generated-by", no co-author trailers).
- `main` is the branch that ships; work branches are free-form (`smca`) and PRs
  merge into `main`.
- `CHANGELOG.md` is written by hand for now (`## [x.y.z]` section per release); the
  in-app changelog screen reads it, and `CMakeLists.txt` embeds it at configure
  time — deleting it breaks the configure step.

## Resources

- XML layouts live in `resources/xml/{activity,tabs,view}`; ids are paths
  (`setting/network/user_agent`) bound with `BRLS_BIND`.
- Icons are SVG, registered by name (`icon/ico-play.svg` → `setImageFromSVGRes`);
  fonts are the Nintendo system fonts under `resources/font`.
- The app icons and splash logo (`resources/icon/icon.{jpg,png}`,
  `icon-rounded.png`, `logo-ghost.png`) are generated from `brand/logos/logo.jpg` by
  `scripts/make-icons.py` — regenerate, never hand-edit. `icon.jpg` must stay a
  256×256 baseline JPEG with no metadata: the HOME menu refuses anything else for the
  forwarder tile (`brand/README.md`).
- Anything the app reads at runtime from the NRO is packed as romfs by the
  `elf2nro --romfsdir` step in `CMakeLists.txt`.
