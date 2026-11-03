# Background update preparation and hardware review

This follow-up is local. It does not authorize a release, installation on a real
console, production changes or player announcements.

## Player flow

Choose **Prepare / resume** in Account > Updates. Downloading, SHA-256 checks,
resource migration and patch reconstruction run in the existing lower-priority
worker while the current version keeps rendering, playing audio and processing
input. **Pause** retains downloaded data; **Keep playing** returns to the island.
Closing the app cancels preparation and retains resumable artifacts and completed
resource objects. Sleep pauses background work instead of being prohibited.

When preparation succeeds, the state becomes **Update ready**. The worker does
not install anything automatically. **Install now** rechecks the staged
application and performs the existing protected installation. That final phase
still pauses gameplay and requires reopening the application. Explicit resource
repair remains a foreground operation, including a bootstrap installation whose
required resource set is incomplete.

The bridge still copies about 276 MB during its first migration. Preparing that
copy gradually avoids a mandatory wait before starting preparation, but does not
remove its SD-space, read, write or checksum costs. A room whose required resources
are absent cannot become playable through scheduling alone.

## Scheduling and integrity

The main loop reports loading activity, frame intervals, model family, battery
saver and suspension. Loading foreground assets holds update work for 200 ms;
frames over the model's budget plus a 6 ms margin hold it for 100 ms. Continued
pressure extends the hold. Hashing, copying, bundle extraction, reconstruction
and downloading all reach cooperative boundaries. Work resumes after pressure
subsides. A user pause is distinct from an automatic hold.

Initial cooperative throughput caps are 768 KiB/s for Old models, 1,536 KiB/s for
New models and 384 KiB/s with Battery saver. They are work-budget ceilings, not
promised download speeds or calibrated physical-console optima. They count
verification and copy work as well as downloaded bytes. Existing two-connection
256 KiB HTTP ranges, ordered writes, sequential fallback, SHA checks and resumable
files remain. A transfer interrupted after yielding can retry from the written
offset once without restarting preparation. Socket or server failures beyond
that leave the ordinary resume action available.

Current resources remain bound to the current application's embedded index.
Preparation creates immutable objects for the next version, rather than switching
the active index or replacing loaded art. Application hashes and identity are
checked during preparation and again before explicit installation.
Before delayed installation, the retained target index is hashed
again and every required object must still exist with the expected size. Missing
objects or a damaged index stop installation and offer preparation again. As with
the boot check, same-size object corruption still requires checksum repair.
Account files are separate. Verified objects and prior versions remain available for rollback.

## CPU, graphics and battery choices

| Concern | Current decision | Evidence and remaining work |
| --- | --- | --- |
| Old 3DS / 2DS | Retain the 30 FPS cap, existing art and a lower-priority worker. Yield during loading and missed frame budgets. | Native Old-model emulator replay plus host cancellation and pacing tests. Physical CPU/SD contention still needs measurement. |
| New 3DS / New 2DS | Retain the 60 FPS normal mode and OS-controlled CPU speedup. | Native New-model replay; emulator timing is not a hardware power measurement. |
| Battery saver | Retain 30 FPS and normal CPU speed; lower the update-work budget. | A separate New-model saver replay. Longer radio activity can offset savings, so no battery-life percentage is claimed. |
| Idle resource worker | Replace the 20 ms polling loop with a signalled event. | Host test observes zero periodic wakeups while idle and verifies request, stop and signal-before-wait handling. This removes that worker's former 50 idle polls per second. |
| GPU work | Retain texture batching, original draw order, clipping, blending and the single used-vertex-range flush. Add Citro3D submission/drawing telemetry. | No graphics format, asset resolution, filtering rule or render order was changed by this follow-up. |
| Memory | Keep bounded update buffers and the adaptive clothing cache with its linear-memory reserve. | Cache and loader sanitizer tests retain the original budgets and cancellation behavior. |
| Additional cores | Keep the auxiliary-core experiment disabled. | Extra cores and system-core quotas are not universally available. A hardware experiment must justify a change. |

The existing renderer already avoids several costly operations: cached name
glyphs, texture batching, asynchronous clothing decode, adaptive eviction and
deferred animation preparation in selected games. Those mechanisms should be
retained and measured before introducing another cache. More texture compression
or smaller banks may save memory but can change alpha, gradients or text, so they
need image comparisons and per-bank budgets. Reordering transparent draws merely
to reduce texture switches can change the picture and is not an acceptable
general optimization.

The frame CSV now includes GPU drawing time, CPU submission time and update state,
byte progress and background status. Existing synchronous room/movie-bank loading
remains a transition bottleneck. A further optimization would stage CPU decoding
outside the render thread and retain main-thread GPU upload. It needs per-bank
readiness and cancellation, peak-memory measurements on Old models, and pixel
comparisons before replacing the current loader.

Existing frame-time percentiles and worst
frames remain; an average FPS alone cannot show a loading stall. Hardware tests
should record sustained crowded rooms, paired multiplayer games, audio underruns,
SD speed and free memory, then compare energy over the same workload. Emulator
CPU quotas and model flags do not reproduce radio current, SD latency, thermal
behavior or battery life.

## Primary implementation references

- [libctru thread priorities and core availability](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/thread.h)
- [libctru OS-controlled speedup](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/os.h)
- [libctru sleep and HOME lifecycle](https://github.com/devkitPro/libctru/blob/master/libctru/source/services/apt.c)
- [Citro3D frame synchronization and CPU/GPU timers](https://github.com/devkitPro/citro3d/blob/master/include/c3d/renderqueue.h)
- [Azahar application-service implementation](https://github.com/azahar-emu/azahar/blob/master/src/core/hle/service/apt/apt.cpp)

## Reproduction

`node tests/update-background.mjs` checks preparation without replacement,
explicit commit, model/saver pacing, pause, loading pressure, sleep, corrupt staged
files, cancellation and cross-process Range resume. The existing CIA installer,
HTTP incremental update, manifest, resource-store and loader checks still apply.

`python3 tools/run-background-review.py` runs isolated native replays for Old,
New and New Battery saver, each with a control and background-preparation case.
It uses synthetic update artifacts, a real embedded resource for migration, an
isolated loopback server and no installation. All 24 scenes must reach their
readiness checks. The synthetic CIA retains a valid matching title/version header
so the normal verifier runs; it is never an installable release candidate.

Before running this script, build its LAN-only client with:

```sh
bash build.sh LAN_TEST=1 GP_UPDATE_TEST_URL=http://127.0.0.1:18891/latest.json \
  TARGET=gopenguin-background-final-test BUILD=obj-background
```

Sanitizer checks use the existing `native-3ds-tests` container when host sanitizer
libraries are unavailable. Public builds exclude all automatic test controls and
must use the public update endpoint.
