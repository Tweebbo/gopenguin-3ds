#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
web=${GOPENGUIN_WEB:-$HOME/gopenguin-dev/web}
mkdir -p build
cc -O2 -std=c11 -Wall -Wextra -Werror -Isource tests/game-fire.c source/game_fire_model.c -lm -o build/game-fire-test
FIRE_REFERENCE="$web" node tests/game-fire.mjs
