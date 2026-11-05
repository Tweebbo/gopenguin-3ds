#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
flags=(-O2 -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Isource -Ibuild/host-include)
output=build/dual-screen-music
if [[ "${SANITIZE:-0}" == 1 ]]; then
 flags+=(-g -fsanitize=address,undefined -fno-omit-frame-pointer)
 output+=-sanitized
fi
mkdir -p "$output"
for game in dj3k dance; do
 cc "${flags[@]}" "tests/game-$game-port.c" "source/game_$game.c" "source/game_${game}_model.c" \
  source/game_port.c tests/game-controller-stubs.c tests/game-bank-host.c \
  source/package.c source/hash.c source/movie_meta.c source/swl.c \
  -lcrypto -lz -l:libjansson.so.4 -lm -o "$output/$game"
 "$output/$game"
done
