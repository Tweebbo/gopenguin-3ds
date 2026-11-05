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

Add `--lossless` to compress the immutable packaging snapshot before making the
CIA and 3DSX. The original prepared RomFS and its pinned profiles remain intact.
The packer preserves a `romfs-source` copy, verifies source stability, records
every optimized file hash and validates all embedded files in both artifacts.
GPK2 uses the existing zlib decoder for each scene, metadata and Tex3DS part.
PCM keeps its existing independent 8 KiB blocks, predictors and decoder. Texture
dimensions, pixels, format, decoded sizes and PCM samples are unchanged. This
does not lower raster density or change the 64 MB Legacy memory configuration.

`tools/optimize-romfs.py --source romfs --output build/lossless-romfs --report
build/lossless.json` stages the same transformation separately. The output must
be a new directory. Python zlib level 9 is always available; installed
libdeflate additionally tries level 12 on the build machine and keeps a result
only when it is smaller. The console does not gain another codec or library.
Compressed parts are decoded and compared before publication into the stage;
incompressible files retain their original bytes. Optimize before constructing
any split resource index, since container hashes and sizes change.

The 2026-11-05 local stage reduced resource bytes from 378,651,450 to
354,372,661. ASan/UBSan checks through `package.c` and `pcm_stream.c` compared
728 optimized movie packages and 617 audio files against the originals,
including audio rewind. This establishes lossless decoding, not physical SD
performance. Zopfli was evaluated separately; its slower build cost was excluded
from the default pipeline. References: [libdeflate](https://github.com/ebiggers/libdeflate),
[Zopfli](https://github.com/google/zopfli), and
[Tex3DS compression options](https://github.com/devkitPro/tex3ds).

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

### 3DS power profiles (local candidate, 2026-11-05)

`source/power_profile.h` selects and applies the runtime profile. Old 3DS remains at 30 FPS and its normal CPU clock. New 3DS normal mode requests 804 MHz plus L2 and up to 60 FPS during activity. After two quiet seconds it requests the normal clock plus L2 and 30 FPS. Battery saver keeps that latter profile during gameplay too. Input immediately restores the active normal profile. The blocking updater uses 10 FPS and respects the selected CPU policy. Simulation continues using elapsed time; texture formats, texture budgets and Old 3DS memory mode are unchanged.

CIA service access includes `ptm:sysm`. The application checks the result of `PTMSYSM_ConfigureNew3DSCPU`; a failure is visible in Graphics rather than silently presenting CPU savings as successful. PTM flags 2 select normal clock with L2; flags 3 select high clock with L2. Cached profiles avoid CPU IPC every frame, and wake invalidates the cache because libctru reapplies its saved speedup configuration during resume. Asset workers pause during suspend; update throttling receives the effective low-clock profile, including automatic idle saving, so 30 FPS does not indefinitely defer background transfers.

Primary references: [PTMSYSM CPU flags](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/services/ptmsysm.h), [libctru speedup implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/os.c), [APT resume](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/apt.c), and [Citro3D frame synchronization](https://github.com/devkitPro/citro3d/blob/master/source/renderqueue.c).

`tests/power-profile.c` exercises the real header with SDK call stubs under ASan/UBSan: Old/New, saver on/off, idle/input transitions, foreground updates, normal-clock L2, cached calls, resume, PTM errors and timer resets. Offline Azahar Astro probes measured approximately 60 FPS for New normal, 30 FPS for New saver, and 30 FPS for Old normal; New CPU requests returned success. Evidence is in `~/GoPenguin-Minigame-Review-20261105/checks/power/`. Emulator timings and accepted clock requests do not measure electrical power or battery runtime. Physical A/B acceptance must use equal brightness, volume, Wi-Fi, battery condition and workload, including sleep/resume and active minigames.

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

## Local mission 2 and minigame preparation work

This unpublished candidate adds G's Secret Mission alongside Case of the Missing
Puffles. Both are available through the PSA selector. Mission 2 uses the matching
local PC/server source in the sibling `web-server/` workspace. The public server
has not been changed. The version remains 1.3.6; these files are not a release.

The server owns the riddle, item combinations, wilderness progression, prototype
sled model, rescue and rewards. The native adapter keeps one mission scene bank
resident and releases it before opening another. Each of its twelve scene/guide
banks is at most 6 MiB of texture memory. The prototype bank uses RGBA4 at a 0.4
raster scale. Its 21 independently animated track segments and landscape remain
addressable after export, so the controller can position them from server state.
The scene and guide artwork use the original English resources. Native input uses a
five-letter word editor, paged choices, item combinations and stylus steering.

Regenerate converted resources in the companion `web/` directory:

```sh
WADDLE_AUDIT=/path/to/Waddle-Forever-audit python3 -I tools/import-agent-assets.py
WADDLE_AUDIT=/path/to/Waddle-Forever-audit node tools/import-mission2.mjs
python3 -I tools/import-mission2-ride.py
node tests/export-agent-trace.mjs
```

Then, from this native source:

```sh
GOPENGUIN_WEB=/path/to/web-server/web node tools/export-agent2.mjs
python3 tools/check-agent-assets.py
bash tests/agent2-controller.sh /path/to/mission2-trace.json
SANITIZE=1 bash tests/agent2-controller.sh /path/to/mission2-trace.json
bash tests/minigame-render.sh
bash tests/pizzatron-input.sh
bash build.sh
```

The normal build rejects missing, corrupt, misidentified or oversized mission 2
banks. GPU-only regression fixtures do not require mission content. LAN/debug
RomFS stages must include the new banks too. Keep emulator integration isolated
from public worlds, as with the existing gameplay fixtures.

The native controller replay covers 158 server snapshots, all scene transitions,
the word editor's MOGUL packet, guide paging/close, map and inventory controls,
camera, steering/poll and reward packets, valid touch rectangles, render traversal and the
6 MiB limit, including ASan/UBSan. It is a host check, not console acceptance.

Cart Surfer, Hydro Hopper, Puffle Roundup, Thin Ice and Sled Racing now defer the
final timeline preparation until controller edits have been applied. Repeating
the same explicit visibility value no longer invalidates a prepared scene. The
eager API stays available for callers that inspect newly prepared animation
state immediately, including Ice Fishing and Bean Counters.

`tests/minigame-render.sh` compares the real controllers, the same seeded input
and original banks, at 24 Hz for 480 frames with two screen transforms. The table
compares eager and deferred preparation paths in the fixture. Both paths include
the idempotent visibility setter. Their draw-command hashes match at complete
logic frames, including masks, colours and text.

| Game | Eager preparations | Deferred preparations |
| --- | ---: | ---: |
| Cart Surfer | 961 | 483 |
| Hydro Hopper | 1438 | 962 |
| Puffle Roundup | 958 | 482 |
| Thin Ice | 920 | 463 |
| Sled Racing | 963 | 483 |

These are CPU-work counts. They are not FPS, GPU timings, battery-life results or
physical Old 3DS measurements. Between-tick interpolation can use the previously
displayed scene after batching; complete-frame hashes do not prove identical
intermediate GPU pixels. Physical Old/New 3DS frame pacing, input feel, audio,
suspend/resume and the reduced prototype raster need acceptance before release.
Original mission audio, companion-puffle animation and full cutscene fidelity
remain unfinished. Missions 3–11 and the other missing standalone games remain
separate ports; this work does not mark them complete.

## Reviewed 3DS sharpness profile

`assets/sharpness/profile.json` pins sharper exports for Town, Coffee Shop,
Book Room, Pizza Parlor, Dance Lounge, Gift Shop, Ski Village, Ski Lodge and
Lodge Attic, plus the Cart Surfer and Pizzatron main banks. Room backdrops use
two-thirds source density instead of half density. Each room still uses one
2 MiB RGBA8 page. Each game main bank still uses six RGBA4 pages (6 MiB), with
its existing 2 MiB title bank unchanged. Runtime linear filtering is retained.

`tools/export-game.mjs` keeps SVGs at source resolution during composition.
`tools/export-scene.mjs` combines primitive density and atlas fitting into one
final resample. Stable room batches render at source scale before downsampling,
preserving geometry when the requested density is fractional. The default room
density remains 0.5; `tools/export-sharp-rooms.mjs` explicitly requests 2/3 and
rejects growth beyond each existing room's page count and texture budget.

Normal builds apply `tools/restore-sharpness-assets.py`; RomFS validation checks
all 20 pinned resources. Unknown or missing bases fail before any replacement.
Regenerated files are staged under `build/` and require a new visual review
before updating the pinned profile. Bean Counters and Ice Fishing trials were
excluded because they did not consistently improve the comparisons.

The [local sharpness review](../../review/3ds-sharpness.md) includes source,
before/after comparisons, native GPU screenshots and test logs. Town's four
GPU cases passed in offline Azahar at 400×240 with Old 3DS settings. Pizzatron
input/controller checks passed with the new bank, including ASan/UBSan, and
Cart Surfer retained its 480-frame gameplay model hash. Physical 3DS LCD and
frame-pacing validation remains pending; these checks do not establish FPS gains.

## Offline native screenshots for review

`tests/review_gpu_test.c` draws the real mission 2, Cart Surfer and Pizzatron
controllers through `game_gpu.c` with the packaged movie banks. Its host drawing
callbacks mirror the client's font, Unicode fallback and game buttons. It contains
no network transport. Mission cases use recorded authoritative snapshots; game
cases stop at fixed title/gameplay states. These are presentation captures rather
than a live session or physical-console acceptance.

Stage a recorded mission trace with
`python3 tools/prepare-native-review.py --trace PATH`, then build with
`make REVIEW_GPU_TEST=1` inside the SDK container. The prepared emulator runs
`tools/native-review-emu-session.sh` with networking disabled. From the workspace
root, `review/source/collect-native.py` preserves all raw captures and crops the
400x240 top screen plus the centered 320x240 touch screen without rescaling.
`review/native3ds/provenance.json` records source-state indices, texture memory
and the hashes of the executable, original captures and crops.

The first twelve render cases passed their texture limits. Visual inspection
then exposed a guide mapping error: the first two timeline frames are the small
inventory cover, not printed pages. This was corrected by the printed-page
mapping and native text reader described below. The earlier cover-only capture
is preserved in the review for comparison. Render success alone does not replace
physical-console acceptance.

## Mission 2 dialogue and original word form

The native mission 2 dialogue and response labels use 14px bitmap text. Wrapping
uses the exported font advances in pixels. English smart apostrophes are rendered
with the bitmap apostrophe so a whole response does not fall back to the smaller
system-font path. Long text is paged locally; it does not advance server dialogue.

The word form uses the original English labels from quest 2's `uitext.swf`:
`The word:`, `This is the word!` and
`No, can you give me the riddle again?`. The old five-letter cycling controls
were replaced by an empty editable field. `GameHost.edit_text` opens the platform
keyboard; cancel discards the edited copy, accepted text retains up to sixteen
letters, and an empty submission stays on the form. A also opens the field.
The PC form uses the same labels and original riddle-again action. The server
repeats G's original clue only while the player is at the answer prompt.

The native host checks cover all 158 recorded states, text bounds, long-text
next/previous paging, cancel, typing, filtering, the field limit, empty input
and the riddle-again packet, with regular and ASan/UBSan runs. The full browser
route covers asking for the riddle again, a wrong answer and completion. Updated
offline GPU captures show the new dialogue and form; the HTML includes native
before/after images. Physical 3DS acceptance remains pending.

## Complete readable survival guide

Release 1.3.7 pins the twelve mission banks in `assets/mission2/`. The build
restores them with `tools/restore-agent-assets.py` before checking their package
integrity and 6 MiB texture limit. This allows builds from the published 1.3.0
RomFS base without the original audit checkout. Unknown existing mission banks
are rejected. The release workflow restores seasonal art, mission banks and the
sharpness profile before its RomFS checks.

The original guide has eight printed pages in four spreads, at timeline frames
4–7. Frames 1 and 2 contain only the small inventory cover. Logical pages now map
to `[4, 4, 5, 5, 6, 6, 7, 7]` in both clients.

`web/tools/import-mission2-guide.mjs` recovers all 23 nonnumeric text blocks from
the pinned original SWF extraction. It checks that every original text block is
accounted for, preserves the printed reading order and writes the shared PC data
and `source/mission2_guide.h`. Original wording and spelling are retained; native
display only normalizes smart apostrophes and whitespace for the bitmap font.

The 3DS top screen shows the original illustrated spread. The touch screen shows
all text for one printed page at 14px, in seven-line parts. Next/Previous traverse
local parts before requesting another printed page; L/R do the same and B closes
the book. The first and last bounds are disabled correctly. No texture bank was
changed or enlarged; the original guide still uses 6 MiB of textures.

The controller checks compare every displayed word across all eight pages with
the recovered source. All thirteen readable parts match, with regular and
ASan/UBSan runs. `build/agent-tests/guide-readability.json` records the page,
spread frame and part count. The browser checks all eight logical page mappings.
Use `tools/prepare-native-review.py --trace PATH --guide-pages` after the host
checks to stage the twelve main cases plus every guide part for native capture.
The HTML review embeds all captures and the complete original transcript. LCD
appearance and reading comfort still need a physical 3DS check.
