#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
web=${GOPENGUIN_WEB:-$HOME/gopenguin-dev/web}
host=${HOST_INCLUDE:-$PWD/build/host-include}
mkdir -p build
cc -O2 -std=c11 -Wall -Wextra -Werror -Isource tests/game-astro.c source/game_astro_model.c -lm -o build/game-astro-test
ASTRO_REFERENCE="$web" node tests/game-astro.mjs
cc -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Wno-misleading-indentation \
  -Isource -I"$host" tests/game-astro-port.c source/game_astro.c source/game_astro_model.c source/game_port.c \
  tests/game-controller-stubs.c tests/game-bank-host.c source/package.c source/hash.c \
  source/movie_meta.c source/swl.c -lcrypto -lz -l:libjansson.so.4 -lm -o build/game-astro-port
TEST_ROMFS=romfs build/game-astro-port
