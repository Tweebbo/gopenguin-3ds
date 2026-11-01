#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
mkdir -p build/wear-tests
temp=$(mktemp -d)
trap 'rm -rf "$temp"' EXIT
python3 tests/fixtures/wear-files.py "$temp"
flags=(-D_POSIX_C_SOURCE=200809L -std=c11 -g -O1 -Wall -Wextra -Werror
  -fsanitize=address,undefined -fno-omit-frame-pointer -Itests/stubs -Isource)
hash_flags=();crypto=-lcrypto
if [ "${HASH_BACKEND:-openssl}" = mbedtls ]; then hash_flags=(-D__3DS__ -Wno-deprecated-declarations);crypto=-lmbedcrypto;fi
cc "${flags[@]}" "${hash_flags[@]}" -c source/hash.c -o build/wear-tests/hash.o
cc "${flags[@]}" \
  tests/wear-loader.c source/wear_loader.c source/wear_texture.c source/garment.c \
  source/sprite.c build/wear-tests/hash.o source/package.c -Wl,--wrap=hash_file_cancel -Wl,--wrap=pthread_create \
  -lpthread "$crypto" -lz -o build/wear-tests/loader-test
cc "${flags[@]}" -ffunction-sections -fdata-sections \
  tests/wear-cache.c source/wear.c source/wear_index.c source/wear_loader.c source/wear_texture.c \
  source/garment.c source/sprite.c build/wear-tests/hash.o source/package.c \
  -Wl,--gc-sections -lpthread "$crypto" -lz -lm -o build/wear-tests/cache-test
cd "$temp"
"$root/build/wear-tests/loader-test"
"$root/build/wear-tests/cache-test"
