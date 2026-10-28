# Shared native transport

`conn.c/.h`, `ws.c/.h` and `certs.c/.h` were copied from the owner's
`Tweebbo-3DS/source` on 2026-10-26. They retain the existing TLS implementation,
certificate verification, HTTP framing and WebSocket handling. GoPenguin supplies
its own lifecycle, credentials, queues and XT protocol in `penguin_net.c`.
No hotel production URL, saved Tweebbo token or public updater configuration is
copied into the application. The shared `common.h` points at GoPenguin storage.
