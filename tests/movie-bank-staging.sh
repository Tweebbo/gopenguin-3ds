#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
cc -O1 -g -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
  -Itests/movie-bank-staging -Itests/stubs -Isource \
  tests/movie-bank-staging.c -Wl,--gc-sections -o build/movie-bank-staging-test
ASAN_OPTIONS=detect_leaks=1 build/movie-bank-staging-test
