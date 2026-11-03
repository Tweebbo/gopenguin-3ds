#!/usr/bin/env bash
set -euo pipefail
export DISPLAY=:99 QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2
export XDG_CONFIG_HOME=/emu/home/.config XDG_DATA_HOME=/emu/home/.local/share
out=/emu
sd=/emu/home/.local/share/azahar-emu/sdmc/3ds/gopenguin
Xvfb :99 -screen 0 1280x1024x24 > "$out/xvfb.log" 2>&1 &
sleep 2
/azahar/AppRun "${PERFORMANCE_APP:-/work/out/gopenguin-lan-test.3dsx}" > "$out/azahar.log" 2>&1 &
emulator_pid=$!
trap 'kill "$emulator_pid" 2>/dev/null || true' EXIT
ready=0
for attempt in $(seq 1 2400); do
  kill -0 "$emulator_pid"
  if [[ -s "$sd/performance-case" ]]; then ready=1; break; fi
  sleep 0.1
done
if [[ "$ready" != 1 ]]; then import -window root "$out/startup-failure.png"; exit 1; fi
window=$(xdotool search --onlyvisible --class azahar | head -n 1)
xdotool windowmove "$window" 0 0 windowsize "$window" 400 526
xdotool windowfocus "$window"
for step in $(seq 1 24); do
  for attempt in $(seq 1 400); do
    kill -0 "$emulator_pid"
    current=0
    read -r current < "$sd/performance-case" || current=0
    [[ "$current" =~ ^[0-9]+$ ]] || current=0
    if ((current>=step)); then break; fi
    sleep 0.1
  done
  if ((current!=step)); then printf 'Replay skipped case %s.\n' "$step" >&2; exit 1; fi
  sleep 1.5
  kill -0 "$emulator_pid"
  import -window root "$out/case-$step.png"
  printf 'Captured performance case %s.\n' "$step"
done
for attempt in $(seq 1 400); do
  [[ -s "$sd/performance-done" ]] && break
  kill -0 "$emulator_pid"
  sleep 0.1
done
[[ -s "$sd/performance-done" ]]
cp "$sd/performance.csv" "$sd/performance-cases.csv" "$out/"
printf 'PASS: offline full-client replay finished; this is emulator evidence.\n' > "$out/result.txt"
