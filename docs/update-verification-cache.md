# Resource verification and optional compressed downloads

This local follow-up retains GoPenguin's existing background preparation,
pause/resume, two-connection range downloader and explicit foreground install.
No release or production manifest was changed.

## Avoid repeated SD reads

Normal updates use a 72-byte verification receipt beside each immutable resource
object. A receipt is written only after the object's content matches the SHA-256
in the authenticated target index. It binds that digest and expected size to the
object's available filesystem modification/change timestamps. Missing, malformed
or stale receipts trigger a full checksum pass. A changed target index selects
the object by its new expected hash; a receipt for a different object cannot
authorize it.

An explicit resource repair bypasses every receipt and hashes all required
objects. If an object fails that check, it is removed from the ready set before
repair chooses its download. Previously a same-size corrupt object could remain
present after failed verification, causing the size-only readiness check to
mistake it for a complete resource set.

Copies from the installed resources and extracted bundle objects are hashed as
they are written, and writes/close failures still stop preparation. Their old
second read of the newly written temporary file is removed. Object hashes and
the whole bundle hash remain mandatory. A cancelled or interrupted operation
retains already completed valid objects; no receipt is issued for a partial
object.

The cache adds no persistent in-memory lookup table or new I/O heap buffer. The
existing 128 KiB resource I/O buffer and worker stack limits remain unchanged;
receipt handling uses bounded stack records and ordinary short-lived file I/O.
There is one small sidecar file per object, so SD allocation overhead depends on
the card's cluster size.

Normal cache reuse is not a full SD integrity scan. Silent same-size corruption
that leaves available filesystem timestamps unchanged requires **Repair
resources**. The host tests can inspect nanosecond change times; 3DS timestamp
precision is coarser. This limitation must remain visible in technical reports.

## Measurements

The retained 1,275-path release fixture contains 276,230,734 resource bytes.
After initial verified migration, normal reuse schedules zero resource content
bytes for hashing, whereas explicit repair still verifies all 276,230,734 bytes.
One host run took 0.019 seconds for cached reuse and 0.464 seconds for full repair.
Those are development-machine timings, not 3DS SD/Wi-Fi performance promises.
The substantive improvement is eliminating that repeated 276 MB read/hash pass.

`tests/resource-cache.py` measures work through the actual cooperative byte
counter and covers first migration, unchanged reuse, changed object metadata,
restored host mtime, damaged receipts, a changed index and strict repair. It also
checks the immutable real release snapshot instead of the actively changing
development RomFS. Background, resource-updater, incremental and CIA worker
regressions remain separate checks.

## Lossless full-package alternative

`tools/package-zcia.py INPUT.cia OUTPUT.zcia --report report.json` creates an
optional standard Z3DS v1 compressed CIA while retaining the original full CIA.
It uses independent 256 KiB seekable Zstandard frames and verifies every frame
plus a complete round trip through the independent `zstd` executable. The
decoded SHA-256 must equal the source CIA exactly; no texture, audio or executable
byte changes.

The format follows the [Azahar Z3DS specification](https://github.com/azahar-emu/azahar/pull/1208).
The current [3dsDL reader](https://github.com/ankushKun/3dsDL/blob/68d7a8032491c09ced53ecb5b8977b5374235318/source/core/z3ds.cpp)
supports this wrapper and seek table. The separately compiled upstream reader is
used for compatibility checks; no upstream implementation was imported into
GoPenguin. `ZSTD_estimateDStreamSize(262144)` reports 751,416 bytes for the
streaming decoder state/buffers alone; an installer also needs its own input,
output and seek-table memory. This optional format does not add a decoder or
change GoPenguin's runtime memory budget.

Measured existing candidates:

| Candidate | Full CIA | Optional ZCIA | Saving |
| --- | ---: | ---: | ---: |
| Tweebbo LAN debug | 88,392,640 bytes | 54,502,410 bytes | 33,890,230 bytes (38.34%) |
| GoPenguin lossless minigame candidate | 358,364,096 bytes | 327,099,084 bytes | 31,265,012 bytes (8.72%) |

These artifacts are additional compressed downloads for compatible 3dsDL or
Azahar installation. They are not selected by the current in-game updater, and
they are not public releases. A physical-console install through 3dsDL remains
to be checked. The CIA itself remains available as the compatibility alternative.
