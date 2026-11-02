#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
flags=(-O2 -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Isource)
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-g -fsanitize=address,undefined -fno-omit-frame-pointer); fi
mkdir -p build/agent-tests
cc "${flags[@]}" tests/agent2-controller.c source/game_agent2.c source/game_port.c tests/game-controller-stubs.c tests/game-bank-host.c source/package.c source/hash.c source/movie_meta.c source/swl.c -lcrypto -lz -ljansson -lm -o build/agent-tests/controller
build/agent-tests/controller "${1:?Pass the authoritative mission2-trace.json path}"
