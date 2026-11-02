#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
for test in puffles puffle-motion puffle-actions puffle-arrival; do
 cc -O2 -std=c11 -Wall -Wextra -Werror tests/$test.c source/puffles.c source/demo.c source/actor_index.c -lm -o build/$test-test
 build/$test-test
done
cc -O2 -std=c11 -Wall -Wextra -Werror tests/cave-mine.c source/cave_mine.c -o build/cave-mine-test
build/cave-mine-test
