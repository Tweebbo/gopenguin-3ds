# Build and test

Use devkitARM, libctru, citro2d, citro3d, Jansson, mbedTLS and zlib.

The prepared `romfs` folder is distributed as `gopenguin-build-assets.tar.gz` in release 1.3.0. Extract it in the repository, then run `make` in a configured devkitPro shell. `build.sh` provides the container equivalent.

CIA packaging uses `node tools/package-native.mjs --release --tools /path/to/ctrtools` with makerom 0.18.4. makerom 0.19.0 has a known minor-version encoding issue.

Update formats are described in [UPDATER.md](UPDATER.md) and [INCREMENTAL-UPDATES.md](INCREMENTAL-UPDATES.md).
