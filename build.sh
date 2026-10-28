#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
test -d romfs || { echo 'Extract gopenguin-build-assets.tar.gz from the release first.' >&2; exit 1; }
podman run --rm --network none --cpus 2 --memory 1g -v "$PWD":/work:z -w /work docker.io/devkitpro/devkitarm:latest make -j2
