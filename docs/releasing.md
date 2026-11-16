# Releasing SMCA

SMCA ships one artifact: `SMCA.nro`, built by `.github/workflows/switch.yaml`.

## Cutting a release

1. Bump the version in `CMakeLists.txt` (`VERSION_MAJOR` / `VERSION_MINOR` /
   `VERSION_ALTER`). The tag must match it, e.g. `v0.2.0`.
2. Push the commit to `main`, wait for the `switch` workflow to go green.
3. Tag and publish:

   ```sh
   git tag -a v0.2.0 -m "SMCA 0.2.0"
   git push origin v0.2.0
   gh release create v0.2.0 --title "SMCA 0.2.0" --notes "…" path/to/SMCA.nro
   ```

The in-app updater reads `releases/latest` of the repo named in
`app/include/utils/config.hpp` (`AppVersion::git_repo`), so the release must be
**published, not a draft**, and must contain `SMCA.nro`.

## Portlibs

The build pins exact portlibs (mpv 0.36, ffmpeg 7.1.5, curl 8.16 …) because the
Switch build is sensitive to those versions. They are fetched from a `ci-deps`
prerelease — currently the upstream GMCA one, see the `DEPS_URL` TODO in
`switch.yaml`. To host them yourself:

1. Run the `switch-portlibs` workflow (manual dispatch): it rebuilds every package
   in `scripts/switch/` and publishes a `switch-portlibs` prerelease.
2. Upload those `*.pkg.tar.zst` files to a `ci-deps` **prerelease** of this repo,
   keeping the same file names.
3. Point `DEPS_URL` (in `switch.yaml` and `scripts/build-switch.sh`) at it.

Keep `ci-deps` a prerelease: the updater reads `releases/latest`, and a release
without `SMCA.nro` in it would break updates.
