#!/usr/bin/env bash
set -euo pipefail
export DISPLAY=:99 QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2
export XDG_CONFIG_HOME=/emu/home/.config XDG_DATA_HOME=/emu/home/.local/share
sd=/emu/home/.local/share/azahar-emu/sdmc/3ds/gopenguin
Xvfb :99 -screen 0 1280x1024x24 > /emu/xvfb.log 2>&1 &
sleep 2
started=$SECONDS
/azahar/AppRun /work/build/performance/apps/gopenguin-lan-split-test.3dsx > /emu/azahar.log 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true' EXIT
for attempt in $(seq 1 600); do
 kill -0 "$pid"
 [[ -s "$sd/resources-state.txt" ]] && break
 sleep 0.1
done
grep -qx "enabled=1 complete=$EXPECTED_READY" "$sd/resources-state.txt"
printf 'PASS: complete=%s; emulator boot resource check finished after %ss.\n' "$EXPECTED_READY" "$((SECONDS-started))" > /emu/result.txt
