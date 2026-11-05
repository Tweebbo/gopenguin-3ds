#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/dual-screen-tests
flags=(-O2 -std=c11 -D_POSIX_C_SOURCE=200809L -DGP_NINJA_GAMES -Wall -Wextra -Werror -Wno-misleading-indentation -Isource -I"${HOST_INCLUDE:-$PWD/build/host-include}")
for variant in flow three; do
 extra=()
 controller=source/game_mission.c
 if [[ "$variant" == three ]]; then extra=(-DTEST_MISSION3);controller=source/game_mission3.c; fi
 cc "${flags[@]}" "${extra[@]}" tests/mission-dual-screen.c "$controller" source/game_port.c tests/game-controller-stubs.c tests/game-bank-host.c source/package.c source/hash.c source/movie_meta.c source/swl.c -lcrypto -lz -l:libjansson.so.4 -lm -o "build/dual-screen-tests/mission-$variant"
 "build/dual-screen-tests/mission-$variant"
done
