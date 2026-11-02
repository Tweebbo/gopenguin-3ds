#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/input-tests
flags=(-O2 -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Isource)
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-g -fsanitize=address,undefined -fno-omit-frame-pointer); fi
bank=(tests/game-bank-host.c source/package.c source/hash.c source/movie_meta.c source/swl.c)
port=(tests/pizzatron-fixture.c source/game_port.c source/game_pizzatron_model.c)
libs=(-lcrypto -lz -ljansson -lm)
cc "${flags[@]}" tests/catalogue-touch.c source/package.c source/hash.c source/movie_meta.c source/swl.c "${libs[@]}" -o build/input-tests/catalogue
build/input-tests/catalogue
cc "${flags[@]}" tests/pizzatron-touch.c "${port[@]}" "${bank[@]}" "${libs[@]}" -o build/input-tests/touch
build/input-tests/touch
cc "${flags[@]}" tests/game-pizzatron-port.c source/game_pizzatron.c "${port[@]}" "${bank[@]}" "${libs[@]}" -o build/input-tests/controller
build/input-tests/controller
cc "${flags[@]}" tests/swl-deferred.c "${bank[@]}" "${libs[@]}" -o build/input-tests/render
build/input-tests/render
