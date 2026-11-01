# Build and test

Use devkitARM, libctru, citro2d, citro3d, Jansson, mbedTLS and zlib.

The prepared `romfs` folder is distributed as `gopenguin-build-assets.tar.gz` in release 1.3.0. Extract it in the repository, then run `make` in a configured devkitPro shell. `build.sh` provides the container equivalent.

Before checking the extracted base, run `python3 tools/restore-seasonal-assets.py`. The base predates the reviewed Halloween resources and contains an incomplete HUD atlas. The build also runs this step. `assets/halloween-2010/profile.json` binds the 39 restored resources to their exact published 1.2.5 bytes and records the accepted replacement hashes. Town and Coffee Shop require three texture pages each. `python3 tests/check-romfs.py .` checks both room completeness and the seasonal hashes; checking only file existence cannot detect blank icons or an older room export. Retire or replace this profile explicitly when changing the event.

CIA packaging uses `node tools/package-native.mjs --release --tools /path/to/ctrtools` with makerom 0.18.4. makerom 0.19.0 has a known minor-version encoding issue.

Update formats are described in [UPDATER.md](UPDATER.md) and [INCREMENTAL-UPDATES.md](INCREMENTAL-UPDATES.md).

## Old 3DS resource handling

Clothing metadata, SHA-256 validation and texture decompression run in one lower-priority worker. The queue holds one request and one completed result. GPU allocation, upload and deletion stay on the main thread, inside the frame. Leaving a room or entering a minigame cancels the generation so an old result cannot restore released textures. If thread creation fails, the loader uses the synchronous path. GPK movie construction still runs on the main thread.

The clothing cache uses up to 16 MiB while reserving 8 MiB of currently available linear memory for other systems. It can shrink below the former 8 MiB limit under pressure. Wanted-item priority governs eviction. `wear_free()` releases resident textures before minigame allocation; the cache grows again as memory becomes available.

Call `swl_gpu_commit()` after both screens have finished drawing and before `C3D_FrameEnd()`. It flushes the used vertex range once. Color changes reuse the normal combiner configuration; switching shader or premultiplied group state invalidates that configuration.

Call `swl_gpu_screen()` after selecting each C2D screen. A completed room draw restores that full-screen context too. Clothing movie previews can be embedded after the minimap or on inventory pages; retaining the minimap's clip rectangle would clip subsequent icons, buttons and text. Embedded actors within a room draw continue to use the room's clip and stencil context.

`make` applies `tools/compact-atlas.py` to the prepared RomFS. The checked source PNG regenerates the original `penguin-detail-0` atlas exactly before conversion to RGBA4. The conversion preserves dimensions and UVs and saves 2 MiB of linear memory. Unknown input hashes fail the build rather than replacing a different asset. Other atlases retain their original format.

Run `tests/old3ds.sh`, `tests/wear-loader.sh` and `python3 tests/check-romfs.py .` for host checks. The clothing tests require AddressSanitizer, UndefinedBehaviorSanitizer and OpenSSL development files; `HASH_BACKEND=mbedtls` selects mbedTLS instead. `make GPU_TEST=1 ROMFS=/path/to/gpu-fixture` builds the graphics regression fixture.

An isolated auxiliary-core experiment can be built with `make CORE1_TEST=1 ROMFS=romfs`. It uses `sdmc:/3ds/gopenguin-core1-test`, requests 5% application time on core 1, and falls back to default affinity if unavailable. It restores the previous time limit when the worker stops. This flag is disabled in normal builds and needs physical Old 3DS testing before release. Do not infer GPU time, SD latency or hardware frame pacing from emulator CPU measurements.

`Help > Graphics > Hide other penguins` skips other players' room avatars, names, minimap markers and clothing requests. The local penguin and Captain Rockhopper remain visible. Messages, emotes, chat history, the player list and network updates remain active. The `hideOtherPenguins` setting defaults to false, including when loading older settings files, and persists on SD. Enabling it requests an unwanted-clothing trim; GPU textures are released by the next `wear_update()` inside the frame, preserving resources still requested by the local penguin, guides and room furniture. Disabling it resumes ordinary avatar rendering and clothing loading. Battery saver is also available in the Graphics page.

Chat letters and edit keys register on touch-down; Send and Close require release on the same button. The keyboard caches the last two visible input lines and recalculates wrapping only after an edit or a width-provider change. Wrapping respects UTF-8 boundaries. ASCII text uses the existing bitmap font, with the normal system-font fallback for Unicode. `tests/chat-keyboard.c` checks touch timing, guarded actions, failed-send retention, character limits and layout-cache invalidation.
