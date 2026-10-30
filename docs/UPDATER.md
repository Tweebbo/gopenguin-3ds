# Updating GoPenguin on Nintendo 3DS

Starting with 1.2.2, the updater supports optional smaller downloads. See
[Incremental updates](INCREMENTAL-UPDATES.md) for the bridge-update requirement,
SD space, validation and full-download fallback.

Open **Help > Account > Updates** and choose **Install / resume**.
Keep GoPenguin open until it confirms that installation finished, then press
START to close and reopen it. Your penguin stays in `3ds/gopenguin` on the SD card.

Starting in 1.1.5, the screen distinguishes download, checksum verification,
installation and finalization. Verification and installation have their own byte
progress; finalization shows elapsed seconds. Reaching the download's final MB
does not mean installation is complete. The version shown as **Running** changes
after reopening the app; **Installed** identifies the update just completed.

An update started from an older version still uses that older updater's screen
until the new app is opened. Older versions can show a full download counter and
"Please wait..." during verification and installation.

## Retained download method

The owner requires future updates to retain the Tweebbo method derived from
3dsdl: parallel byte ranges over persistent connections, ordered writes and
resumable downloads. Artifacts of at least 1 MiB use two connections and 256 KiB
range chunks. Preserve cancellation, sequential fallback and validation of size,
SHA-256, Title ID and title version. Console measurements are required before
claiming a download speed improvement.

## Completion and diagnostics

The installed CIA's Title ID and title version are read back from AM after commit
and must match the verified manifest before success is shown. Background release
checks cannot replace the completion message. START cannot close an active
update, and HOME is disabled during CIA installation/finalization.

`3ds/gopenguin/last-update.log` records running/target versions, phase changes and
installer errors without account credentials. Failed installs retain the verified
CIA for another attempt or installation with FBI; retries verify it again.

## Tests

`node tests/update.mjs` covers manifest/package identity, real HTTP interruption,
range resume and atomic 3DSX replacement. Set `GOPENGUIN_PACKAGE` to a prepared
package directory to validate its artifacts.

`node tests/update-cia.mjs` executes the production CIA worker with mocked libctru
services, real file I/O and SHA-256/package validation. Cases include slow commit,
failed/short writes, cancellation, mismatched installed titles/versions, retry
and protection of the completion state. These tests and emulator checks do not
establish successful installation on a physical console.
