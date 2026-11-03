# Igloo navigation on 3DS

The local candidate improves entry from ordinary rooms and games. Publication and physical-console acceptance are separate steps.

## Loading and recovery

- An igloo bank uses a foreground asset request. It has one reserved queue slot beyond the 32 normal requests and can interrupt an unrelated HTTP download. The interrupted job returns to its original FIFO position, without a failed-download cooldown. Temporary files are discarded before retrying.
- Waiting requests retain their queue sequence when callers poll them. Last-use timestamps still serve cache replacement.
- Repeated navigation to the same owner keeps the active request and deadline. A new owner replaces the target; replies for another owner are rejected while loading.
- Missing metadata is retried after 2 and 6 seconds. Metadata waits stop after 20 seconds. Required-resource waits stop after 30 seconds from the first accepted layout. Duplicate layouts do not extend that resource deadline.
- A loaded igloo movie bank, geometry and navigation can be reused when the style matches and room textures are ready. Furniture, floor, music, owner and spawn come from the current server layout. Switching styles still loads the required bank.
- Catalogues close when igloo navigation starts. An ordinary room change, a disconnect or a failed entry clears the pending visit.
- Game exits continue through the existing game lifecycle. Coin settlement and its no-resend rule are unchanged. Unsaved igloo edits and pending saves still block leaving.
- Background update preparation yields during the metadata wait as well as during resource loading.

An unreachable server cannot provide an online igloo. A timeout leaves the current room usable and permits a new request; it does not replace the player's saved layout with an invented default.

## Verification

`tests/asset-cache-priority.py` exercises the real worker, socket transport and package validation using a loopback HTTP server. It covers full-queue admission, a stalled request, recovery of preempted work, FIFO ordering under repeated polling, malformed packages, 404 replies, cooldown, disk reuse and temporary-file cleanup. `tests/igloo-visit.c` verifies deadlines and disconnect recovery. `tests/igloo-layout.c` verifies selected-owner filtering and preservation of furniture, floor, style, music, dirty edits and verified saves. The host cases also run with AddressSanitizer and UndefinedBehaviorSanitizer.

`tools/igloo-network-fixture.cjs` starts an isolated real game server plus loopback proxies. `tools/run-igloo-reliability.py` runs native LAN builds in separate Old and New Azahar profiles, with 100% CPU clock and 1x resolution. The first entry uses the touchscreen button. Subsequent cases use LAN-only test controls to exercise the same navigation functions. Each run keeps a frozen binary, protocol log, fault events, screenshots and SHA-256 provenance.

The custom-resource case deliberately uses synthetic style 2 metadata with the original basic bank. It validates scheduling and bank loading, not the appearance of a production custom style. Server accounts and session files belong only to the isolated fixture and are excluded from the review report.

The machine-readable results are in `../../review/igloo-reliability/measurements.json`; the self-contained HTML is `../../review/GoPenguin-updater-review.html`. Durations are single-replay emulator observations. They do not establish physical SD/Wi-Fi latency, battery life or long-session stability. Existing art and resource-index bytes are retained.
