#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/dual-screen-tests
flags=(-O2 -std=c11 -D_POSIX_C_SOURCE=200809L -DGP_NINJA_GAMES -Wall -Wextra -Werror -Wno-misleading-indentation -Isource -I"${HOST_INCLUDE:-$PWD/build/host-include}")
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-g -fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/undersea-waves-touch.c source/game_aqua.c source/game_aqua_model.c source/game_rescue.c source/game_rescue_model.c source/game_waves.c source/game_waves_model.c source/game_port.c tests/game-controller-stubs.c tests/game-bank-host.c source/package.c source/hash.c source/movie_meta.c source/swl.c -lcrypto -lz -l:libjansson.so.4 -lm -o build/dual-screen-tests/undersea-waves-touch
build/dual-screen-tests/undersea-waves-touch
