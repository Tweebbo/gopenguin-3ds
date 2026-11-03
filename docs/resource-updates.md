# SD resource updates

These changes are local candidates. No tag, push, release, workflow dispatch,
production configuration change or announcement is part of this work.

The background-preparation follow-up lets the current complete application keep
running while migration, downloads and verification prepare the next version.
Only explicit installation blocks play. Initial bootstrap preparation and full
resource repair remain foreground operations. See [the hardware and scheduling
review](background-updates.md) for the player flow and measured scope.

## Migration order

1. Build the full 1.3.8 bridge with `RESOURCE_LAYOUT` set to `bridge`. It retains
   the existing resources and includes the new updater. Publish it to both
   manifest channels only after approval and console validation.
2. Build a subsequent release, currently staged locally as 1.3.9, with
   `RESOURCE_LAYOUT` set to `split`. Publish it only to `latest-v2.json`. Keep the
   full bridge on `latest.json` for older clients.
3. Before installing the small application, the bridge verifies the target index,
   copies matching resources from its own RomFS to SD, and verifies those copies.
   It fetches only resources absent from the exact base release. Installation is
   blocked until the full target resource set is present.

The first migration still copies about 276 MB to SD and still needs temporary
space for the application update. A small future CIA does not make this first
migration free. Bulk resources remain on SD after the application becomes small.
Full repair downloads the complete resource bundle and is an explicit action.
Old object versions are retained for rollback; automatic garbage collection is
not implemented. Insufficient SD space stops preparation without installing an
incomplete application. Allow space for new objects, the selected bundle and
temporary application files; existing rollback objects also occupy space.

## Content binding and recovery

The embedded index maps each asset path to its exact SHA-256 and byte size.
Objects live at `resources/<first hex digit>/<sha256>.bin` under the application's
SD data directory. Each application has its own index, so it cannot silently use
another release's art. Account settings are separate from the resource store.

Copies and downloads are hashed, closed, read back, and renamed from `.part`
files. The complete bundle is also checked. Missing base resources after a delta
require explicit full repair. Interrupted transfers retain verified objects and
support HTTP range resume. Bootstrap packages retain Town, core graphics and
update UI so an initial installation can prepare missing resources.

Boot readiness checks file presence and size, as before; checksum verification
runs during preparation and repair. On 3DS, directory reads check sizes in batches
instead of opening every file individually. Same-size corruption is detected by
repair, rather than by the lightweight boot check.

## Local build and packaging

`node tools/release-layout.mjs prepare` reads published release metadata and
downloads only previously published resource indices and manifests to generate
up to four exact-base deltas. It does not publish anything.

`node tools/release-layout.mjs build` builds the selected layout.

`node tools/release-layout.mjs package /path/to/ctrtools` creates and structurally
validates both formats, the resource index and any required bundles. The bridge
index is checked against every indexed file in the packaged RomFS snapshot.

The pipeline changes and `3ds/native-update-routes.caddy` in the private pipeline
worktree remain local. The v2 route must be installed during a future approved
deployment. Existing production serves the legacy manifest only.

## Performance evidence

`performance-enabled` in the SD data directory enables bounded CSV measurements.
The CSV holds model family, phase, FPS, frame-time percentiles, worst frame,
draw time, free linear memory and pending asset jobs. Samples use a 128 KiB RAM
buffer and flush at exit. Remove the flag to disable measurement.

LAN builds can replay nine rooms, an igloo and fourteen game entry scenes with
`performance-auto`. Fishing, Roundup and Pizzatron diagnostic writes are disabled
during measurement. Table games use a test transport and cover board entry;
they do not simulate a complete multiplayer match. Missions use server snapshots.
These controls are absent from production builds.

Old 3DS/2DS and New 3DS/New 2DS emulator profiles are separate. New models use a
60 FPS cap; old models use 30 FPS. Battery Saver uses 30 FPS. Emulator results
cannot establish physical FPS, SD throughput, network speed or CIA install time.
Test both model families on hardware before deciding whether to deploy.
