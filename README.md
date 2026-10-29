# GoPenguin for Nintendo 3DS

Explore the island, chat, dress your penguin and play games in the same world as
[gopenguin.lol](https://gopenguin.lol), including players on PC and mobile.

**[Download the latest release](https://github.com/Tweebbo/gopenguin-3ds/releases/latest)**

## Install

Open FBI, choose Remote Install → Scan QR Code, and scan this image.
The QR always points to the latest CIA.

![GoPenguin CIA installation QR](docs/download-qr.png)

You can also install `gopenguin.cia` with FBI, or use `gopenguin.3dsx` with the
Homebrew Launcher. Keep the `3ds/gopenguin` folder on your SD card when updating.
The game reconnects with your saved penguin automatically.

## Play

- Circle Pad: walk. Touch the room map to choose a destination.
- Press R or touch Chat to write. Press L to read the last 100 messages of this session.
- In history, L/R turn pages, Up/Down scroll a line, A writes and B returns.
- START sends chat, SELECT closes the keyboard, B deletes, X changes case and Y adds a space.
- In the keyboard, L also changes case and R switches letters/symbols.
- Touch Me for clothes and inventory. Help → Account groups transfers, name and updates.
- Your saved penguin connects automatically; the first connection gets a penguin automatically.

## Games

Card-Jitsu and Card-Jitsu Fire are available from the Sensei in the Dojo and
Fire Dojo, or from Help → Games. Get a starter deck from the game menu when
needed. Left/Right inspects cards; A plays the selected card. Hold a card to
inspect its full illustration, release to play, or drag outside to cancel.
Matches and belt progress are saved by the server. Some cinematics and game
audio remain work in progress.

Bean Counters now keeps your penguin's colour in every carrying pose, and
walking puffles follow smoothly without repeatedly switching to standing.

## Continue on another device

In Help → Account → Transfer penguin, choose **To PC**. Scan the QR with your phone,
or open gopenguin.lol and enter the short code in Account → Enter device code.
Confirm your penguin's name to continue there.

To move from PC or phone to 3DS, choose Account → Use on another device on the
website, then enter its code in Help → Account → Transfer penguin → From PC on 3DS.
Only the eight characters are needed; the `GP-` prefix and dashes are optional.
Codes work once and expire after ten minutes. Keep them private. Your penguin,
coins and belongings stay the same, and the previous character stays saved on
the destination device. Change your name in Help → Account → Change name.
Changing your name keeps your connection and penguin ID.

## Battery saver

Help → Battery saver saves your preference on the SD card.
On New 3DS it uses the normal CPU clock and 30 FPS. Turn it off for the faster
CPU and up to 60 FPS. Original 3DS already uses the normal clock and 30 FPS.
Chat and crossplay remain connected in either mode. Actual battery life depends
on your console, battery, brightness and wireless conditions.

Talk to Captain Rockhopper in Town to receive his free background. You do not need to send a suggestion. The background stays in your inventory when you change your outfit.

## Updates

GoPenguin checks for releases in the background and offers updates inside the
game. You can also open Help → Account → Updates. Downloads resume after a
connection drop and are verified before installation. The CIA keeps its stable
Title ID and your saved character.

## Build

Use devkitARM, libctru, citro2d, citro3d, Jansson, mbedTLS and zlib. The prepared
`romfs` input is distributed as `gopenguin-build-assets.tar.gz` with the release.
Extract it in the repository, then run `make` in a configured devkitPro shell.
`build.sh` provides the container equivalent. CIA packaging uses
`node tools/package-native.mjs --release --tools /path/to/ctrtools`.
The 1.1.3 CIA was packaged with makerom 0.18.4; makerom 0.19.0 has a known
minor-version encoding issue. The package checks verify the installed version.

This is an unofficial fan project. Original game artwork belongs to its respective
owners. Third-party source notices are retained in `vendor` and the source files.
