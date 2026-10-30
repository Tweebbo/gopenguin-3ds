# Incremental 3DS updates

Available starting with 1.2.2. Versions through 1.2.1 still download complete
packages. Installing 1.2.2 is a full bridge update for those clients; later updates
can use a small download on installations with this reader.

## Player behavior

Updates remain optional. Players can choose Later and keep their current version.
The update panel shows the selected download size and the complete package size.
An exact matching offer uses a compressed `.tupd` recipe; an older client, unsupported
base version or absent offer uses the original full CIA/3DSX download.

The recipe copies unchanged files from the running application's `romfs:/` and
downloads new executable bytes, metadata and changed assets. It does not require
keeping a previous CIA on the SD card. The reconstructed full package still needs
space on SD, and CIA verification/installation still takes time. This reduces network
traffic, not installed size or installation work.

Interrupted downloads resume through the existing range downloader. Missing or
damaged installed files, an invalid recipe or a checksum failure offer an explicit
Full download action. A failed small update never silently starts a large download.
The current application remains installed until the normal installer succeeds.

## Release preparation

Prepare and validate the normal public packages first, with a new version and the
same Title ID. Preserve the complete artifacts and manifest from each published base.

```sh
node tools/prepare-release.mjs build/package-NEXT build/package-PREVIOUS
```

Up to four previous package directories can follow the target. This command only
prepares local files. Upload all generated `.tupd` files with the complete CIA, 3DSX
and `latest.json`. The manifest keeps its existing keys and adds an optional
`incremental` array with format `tupd1`, exact `from` version and per-format
`url`, `sha256`, and `bytes`. Offers are included only when smaller than the full
artifact. Keep the complete artifacts available for old clients and recovery.

For separate tooling, `tools/prepare-incremental.py --help` prepares offers from
verified public-format manifests. `tools/make-incremental.py --help` builds one recipe.
Neither publishes a release. `RELEASE_NOTES` overrides the short manifest notes.

## Integrity and limits

The common TUPD1 reader binds the source/target versions, CIA/3DSX kind, Title ID,
target size and SHA-256 to the manifest. It verifies the downloaded patch, every
reused file, and the exact output. Path traversal, unsupported formats, trailing
compressed/inflated data, truncated streams and oversized output are rejected.
The output limit is 512 MiB, the operation limit is 65,536, and file reads use
bounded 32 KiB buffers. Cancellation deletes only the reconstruction temporary file.
Normal CIA identity/title-version validation and post-install confirmation follow.

## Validation

```sh
python3 tests/incremental.py
python3 tests/incremental-generator.py
node tests/update.mjs
node tests/update-cia.mjs
node tests/update-incremental.mjs
```

Host tests require Jansson headers/runtime, OpenSSL and zlib. `CPPFLAGS` can supply
the Jansson include directory. The decoder suite covers malformed recipes, hashes,
unsafe paths, damaged/missing source files, cancellation and legacy manifests. The
worker suites execute production updater code with simulated console services;
the HTTP suite exercises real interrupted/resumed downloads and atomic 3DSX updates.

A validation fixture rebuilt the exact published 407,827,392-byte 1.2.1 CIA from
1.2.0 assets with a 3,993,049-byte patch, reusing 403,577,451 bytes in 1,127 files.
That historical pair measures reuse; its old clients cannot apply this new format.
Future sizes depend on how many files change. Host/emulator checks do not establish
physical-console installation success or a hardware download-speed improvement.
