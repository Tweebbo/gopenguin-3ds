#!/usr/bin/env bash
set -euo pipefail
export HOME=/emu/home DISPLAY=:99 QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2
out=/work/build/sharpness/emu
Xvfb :99 -screen 0 1280x1024x24 > "$out/xvfb.log" 2>&1 &
sleep 2
/azahar/AppRun /work/out/gopenguin-sharpness-gpu-test.3dsx > "$out/azahar.log" 2>&1 &
emulator_pid=$!
trap 'kill "$emulator_pid" 2>/dev/null || true' EXIT
sleep 10
window=$(xdotool search --onlyvisible --class azahar | head -n 1)
xdotool windowmove "$window" 0 0 windowsize "$window" 400 526
xdotool windowfocus "$window"
sleep 2
for index in 0 1 2 3; do
  result="$HOME/.local/share/azahar-emu/sdmc/gopenguin-sharpness.txt"
  for attempt in $(seq 1 40); do
    if [[ -f "$result" ]] && grep -qx "PASS case=$index" "$result"; then break; fi
    sleep 0.1
  done
  grep -qx "PASS case=$index" "$result"
  cp "$result" "$out/case-$index.txt"
  import -window root "$out/case-$index.png"
  if [[ "$index" != 3 ]]; then
    xdotool keydown a; sleep 0.15; xdotool keyup a; sleep 1
  fi
done
kill -0 "$emulator_pid"
printf 'PASS: four native GPU cases, native resolution, Old 3DS configuration.\n' > "$out/result.txt"
