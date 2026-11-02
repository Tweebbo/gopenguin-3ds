# Build and test

Use devkitARM, libctru, citro2d, citro3d, Jansson, mbedTLS and zlib.

libctru 2.7.1 or newer is required. Earlier versions can request an application
heap larger than its 96 MiB virtual address region when extended memory is granted.
This can fail before `main()` starts. Updating the Homebrew Menu does not replace
the library statically linked into GoPenguin.

`build.sh` uses the local `native-3ds-libctru-fixed` container and builds it from
`tools/Containerfile.libctru` when absent. The recipe installs libctru through
devkitPro's signed package manager. `DEVKITARM_IMAGE` can select another SDK image.
The Makefile checks the installed package before compiling, including direct
`make` invocations. Its library archive fingerprint invalidates cached objects
when libctru changes. Delete old objects before the first corrected build.

The correction retains the normal 64 MB Legacy CIA configuration. Extended-memory
fixtures are private tests and do not change the memory requested by public packages.

The prepared `romfs` folder is distributed as `gopenguin-build-assets.tar.gz` in release 1.3.0. Extract it in the repository, then run `make` in a configured devkitPro shell. `build.sh` provides the container equivalent.

Before checking the extracted base, run `python3 tools/restore-seasonal-assets.py`. The base contains an incomplete HUD atlas, and `assets/hud-icons/profile.json` binds the restored icons to their exact published bytes. The build also runs this step. `python3 tests/check-romfs.py .` checks room completeness and that hash; checking only file existence cannot detect blank icons. Town and Coffee Shop use one texture page each.

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


## Pizzatron and catalogue touch input

Stylus release applies cheese and toppings immediately, before another pickup can
replace the pending ingredient. Releasing in the header clears the held state.
The selected ingredient can also be placed by tapping the pizza. The first sauce
sample is applied at contact; a consumed-sample flag keeps pouring at the original
24 Hz limit instead of emitting extra sauce for every touch event. Existing game
rules, growing splat masks, recipes, scoring and payout validation remain intact.

Pizzatron uses `swl_tick_deferred()` to combine the final animation preparation with
controller edits. Queries and rendering still prepare dirty scenes on demand.
The ordinary `swl_tick()` path remains eager for other callers. The host fixture
requires three scene preparations per dual-screen game frame instead of four;
this is a CPU-work measurement, not a physical-console FPS claim.

Catalogue taps first test the original exact hit areas. On a miss, a purchase
button within six display pixels may be selected, choosing the nearest edge.
Secrets and navigation receive no extra tolerance. The original confirmation and
server-side purchase processing still handle the selected item.

Run `bash tests/pizzatron-input.sh` with OpenSSL, zlib and Jansson development
packages. `SANITIZE=1` adds ASan/UBSan. This covers the real Bee Antennae catalogue
page, quick drops, header release, short sauce strokes, both game modes, original
bank rendering, score settlement, and eager/deferred render equivalence.
For independent rule parity, compile `tests/game-pizzatron.c` to
`build/game-pizzatron-test`, then run `PIZZATRON_REFERENCE=/path/to/web node
tests/game-pizzatron.mjs`; the reference web tree must include esbuild.


## Outfit actions and Cave Mine

`tools/export-outfits.mjs` reads the same original rule JSON and converted artwork
as the PC candidate. Set `GOPENGUIN_WEB` to that web source tree when regenerating.
The generated manifest binds every bank to its SHA-256. The original 76-frame
penguin supports 45 distinct special poses; native data includes 233 original
clothing items and covers 480 available dance combinations across 37 dance poses.
Eight clothing IDs have no original asset in this archive and use the ordinary
base action. Rules beyond the available penguin artwork also retain the base action.

Special avatar body/detail layers share one texture page for each pose. Garment
pages load only as needed through the existing asynchronous loader and adaptive
16 MiB clothing cache. Each page remains at most 2 MiB; neither all poses nor all
outfits become resident at startup. The three garments with long independent child
timelines keep their native movie timelines instead of truncating their animation
cycles. The existing atlas, icons and normal clothing resource protocol remain valid.

A complete special outfit is shown once its avatar and worn garment resources
are ready, with the ordinary action during loading. Network actions remain base
frames (for example, dance 26); each PC/3DS peer resolves the outfit locally.
A newly displayed pose resets playback; movement and outfit changes resolve the matching action again.
The white RGBA8 body layer uses multiplicative colour tinting, while the existing
A8 base atlas keeps solid tinting. This also preserves the lower-screen controls
when an action preview shares the special avatar texture.

Mining requests require Cave Mine, the original digging area, an exact mining
outfit, owned equipment, the dance action, and an active world connection. One
request may be outstanding, with a timeout and a fresh timer per room visit.
Only server replies update the authoritative balance; earned amounts appear above
the penguin. Old replies cannot animate a different visit, even after returning
to the same room. The companion local server in
`/home/miguelv/GoPenguin-Gameplay-PC-20261101` awards 10 coins normally, a 1% chance
of 100, and five rewards per spot before movement of more than 30 pixels is needed.
Deploying the native client alone does not change an existing server's reward policy.

`tests/outfits-mining.py` compares the C selector against every distinct shared
rule. `tests/outfit-resources.py` exercises all 234 banks through native parsers,
pose loops, hashes and page budgets; compile `tests/outfit-resources.c` with the
host bank adapter first. `tests/mining.c` covers timing, eligibility, pending replies
and stale room visits. The wear loader sanitizer fixture includes both new local
asset kinds and confirms they do not request downloads when absent.

LAN builds alone can consume `sdmc:/3ds/gopenguin/gameplay-step` for the isolated
end-to-end fixture. This driver is excluded from public binaries. The fixture runs
the real companion server and a PC-protocol observer alongside Azahar inside a
container with networking disabled, disposable accounts and an empty data folder.
It checks pizza poses, mining payouts and exhaustion, movement, interruption and
outfit removal. Never point an emulator at a public world.


## Catalogue camera and SD identity recovery

The zoomed catalogue camera filters the Circle Pad through a 20% radial dead zone.
D-pad directions ignore analogue noise. Touch drags move relative to the chosen
centre after four pixels of movement; a stationary touch cannot pull the camera
back or overwrite a deliberate stick pan. In zoom mode, item taps activate on
release and drags do not activate purchases. L/R and the footer zoom button retain
the camera centre. New catalogues start centred, and page bounds constrain panning.
`tests/book-camera.c` covers neutral drift over 60 simulated seconds, tap jitter,
relative drags, stationary holds, input handoff, cancellation and bounds.

A saved session in a valid main settings file takes priority. If the session is
missing or malformed, matching-endpoint recovery files are considered before
creating any guest. Interrupted FAT renames can be recovered from `.rollback`,
`.bak` or `.tmp`; a repair write uses `.repair` so a valid staged file is not
truncated. Recovered settings repair the main file even when the session token has
not changed. A repair retains the known backup rather than replacing it with the
bad previous main file. File contents are flushed and synced before publication.

If saved settings cannot be read and no identity can be recovered, automatic guest
creation is blocked and Account provides the restore path. General settings
changes cannot overwrite that unresolved identity. A genuinely fresh online
installation first checks that SD settings can be saved before requesting a guest.
Account import/new-character actions remain explicit and clear the recovery state
after a successful save. Names, inventory, worn items and coins remain server data;
the SD session selects the same penguin on the next launch.

`tests/settings-recovery.c` covers valid JSON with a missing session, corrupt main
files, recovery candidates, explicit restores, failed flushes and endpoint
isolation. Existing FAT replacement, account persistence and actual main.c account
transfer tests also pass. `checks/emu-camera-save` in the candidate's parent holds
the isolated native integration: camera gestures, confirmed Bee Antennae purchase,
free helmet equip, rename, full client/server restart, backup recovery and refusal
to create a guest when all identity files are damaged. Credentials stay outside the
report and packaged RomFS. Physical SD failure and console feel remain unverified.
