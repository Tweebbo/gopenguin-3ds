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
- Touch Chat or press A: open the touch keyboard. The world stays live while typing.
- START sends chat, SELECT closes the keyboard, B deletes, X changes case and Y adds a space.
- Touch Me: clothes, inventory and your name. Touch your name or Edit to rename.
- Help: account recovery, membership, catalogues and updates.

## Continue on another device

In Help → Account, choose **Use on PC / phone**. Scan the QR with your phone,
or open gopenguin.lol and enter the short code in Account → Enter device code.
Confirm your penguin's name to continue there.

To move from PC or phone to 3DS, choose Account → Use on another device on the
website, then enter its code in Help → Account → Enter device code on 3DS.
Only the eight characters are needed; the `GP-` prefix and dashes are optional.
Codes work once and expire after ten minutes. Keep them private. Your penguin,
coins and belongings stay the same, and the previous character stays saved on
the destination device. Permanent recovery codes and files still work.

## Battery saver

Help → Account → Battery saver saves your preference on the SD card.
On New 3DS it uses the normal CPU clock and 30 FPS. Turn it off for the faster
CPU and up to 60 FPS. Original 3DS already uses the normal clock and 30 FPS.
Chat and crossplay remain connected in either mode. Actual battery life depends
on your console, battery, brightness and wireless conditions.

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

This is an unofficial fan project. Original game artwork belongs to its respective
owners. Third-party source notices are retained in `vendor` and the source files.
