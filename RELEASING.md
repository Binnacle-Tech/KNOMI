# Releasing

Releases are built by GitHub Actions (`.github/workflows/build.yml`).

1. Bump `FW_VERSION` in `src/config.h`, e.g. `"V1.0.2-OP6"`.
2. Commit and push to `octoprint`. CI builds both boards and keeps the `.bin` files as workflow artifacts.
3. Tag the commit with the same version in lowercase and push the tag:
   ```
   git tag v1.0.2-op6
   git push origin v1.0.2-op6
   ```
4. CI checks that the tag matches `FW_VERSION` (it fails if you forgot step 1), builds, and creates the GitHub
   release with `knomiv2-octoprint-firmware.bin` and `knomiv1-octoprint-firmware.bin` attached. Edit the release
   notes on GitHub afterwards if you like.

Tags must look like `vX.Y.Z-opN`. The KNOMI's update check compares the latest release tag against its own
`FW_VERSION` in that format.
