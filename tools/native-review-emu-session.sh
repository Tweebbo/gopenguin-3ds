#!/usr/bin/env bash
set -euo pipefail
export HOME=/emu/home DISPLAY=:99 QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2
out=/work/build/native-review/emu
rm -f "$HOME/.local/share/azahar-emu/sdmc/gopenguin-review.txt"
Xvfb :99 -screen 0 1280x1024x24 > "$out/xvfb.log" 2>&1 &
sleep 2
/azahar/AppRun /work/out/gopenguin-review-gpu-test.3dsx > "$out/azahar.log" 2>&1 &
emulator_pid=$!
trap 'kill "$emulator_pid" 2>/dev/null || true' EXIT
sleep 10
window=$(xdotool search --onlyvisible --class azahar | head -n 1)
xdotool windowmove "$window" 0 0 windowsize "$window" 400 526
xdotool windowfocus "$window"
sleep 2
result="$HOME/.local/share/azahar-emu/sdmc/gopenguin-review.txt"
capture_count=12
if [[ -f /work/build/native-review/case-count.txt ]]; then read -r capture_count < /work/build/native-review/case-count.txt; fi
[[ "$capture_count" =~ ^[0-9]+$ ]] && ((capture_count>=12&&capture_count<=64))
for ((index=0; index<capture_count; index++)); do
  ready=0
  for attempt in $(seq 1 120); do
    kill -0 "$emulator_pid"
    if [[ -f "$result" ]] && grep -q "^PASS case=$index " "$result"; then ready=1; break; fi
    sleep 0.1
  done
  if [[ "$ready" != 1 ]]; then
    import -window root "$out/failure-$index.png"
    printf 'Capture case %s did not render successfully.\n' "$index" >&2
    exit 1
  fi
  cp "$result" "$out/case-$index.txt"
  import -window root "$out/case-$index.png"
  printf 'Captured native case %s.\n' "$index"
  if ((index+1<capture_count)); then xdotool keydown a; sleep 0.15; xdotool keyup a; sleep 0.5; fi
done
printf 'PASS: %s native dual-screen captures, offline controller replay, Old 3DS settings.\n' "$capture_count" > "$out/result.txt"
