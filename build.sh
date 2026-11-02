#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
test -d romfs || { echo 'Extract gopenguin-build-assets.tar.gz from the release first.' >&2; exit 1; }
image=${DEVKITARM_IMAGE:-localhost/native-3ds-libctru-fixed:latest}
if ! podman image exists "$image"; then
  podman build -t "$image" -f tools/Containerfile.libctru tools
fi
podman run --rm --network none --cpus 2 --memory 1g -v "$PWD":/work:z -w /work "$image" make -j2
