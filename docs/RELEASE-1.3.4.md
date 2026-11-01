# GoPenguin 1.3.4 for Nintendo 3DS

This optional update restores the bottom-screen icons and the Halloween rooms, map, poster and music that were missing from recent 3DS packages. Town, Coffee Shop, Dance Club and Gift Shop use the Halloween event artwork again.

Clothing previews also restore the correct screen drawing context, so they no longer clip away icons and controls after the minimap.

Clothing loading now performs file reading, verification and decompression in a background worker. Rendering uses fewer repeated graphics state changes, and the clothing cache adapts to available memory. One penguin atlas uses 2 MiB less texture memory.

In Help > Graphics, you can enable **Hide other penguins** to reduce rendering and clothing memory use. It is off by default. Other players' messages, emotes, chat history and the People list remain available while their avatars are hidden. Your penguin and room guides stay visible.

The chat keyboard registers letters when you touch a key and avoids repeatedly measuring unchanged input. Send and Close still require releasing on the button.

Update from **Help > Account > Updates**. Smaller incremental downloads are available from 1.3.3, 1.3.0, 1.2.5 and 1.2.3, for both CIA and 3DSX. The updater retains the full-download fallback and your saved SD data. Versions 1.3.1 and 1.3.2 cannot reach the updater; install the latest CIA or 3DSX from [Resources](https://gopenguin.lol/resources).

The changes were checked with host tests and an emulator configured as an Old 3DS with 64 MiB application memory. Physical console testing remains outstanding. Crowded rooms can still have long frames, and high outfit variety can exceed the clothing texture budget when everyone is visible.
