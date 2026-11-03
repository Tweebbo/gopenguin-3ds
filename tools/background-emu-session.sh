#!/usr/bin/env bash
set -euo pipefail
export DISPLAY=:99 QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2
export XDG_CONFIG_HOME=/emu/home/.config XDG_DATA_HOME=/emu/home/.local/share
sd=/emu/home/.local/share/azahar-emu/sdmc/3ds/gopenguin
Xvfb :99 -screen 0 1280x1024x24 > /emu/xvfb.log 2>&1 &
sleep 2
/azahar/AppRun "${BACKGROUND_APP:-/work/out/gopenguin-background-test.3dsx}" > /emu/azahar.log 2>&1 &
emulator_pid=$!
trap 'kill "$emulator_pid" 2>/dev/null || true' EXIT
for attempt in $(seq 1 1800); do
 kill -0 "$emulator_pid"
 [[ -s "$sd/performance-case" || -s "$sd/background-ui-case" ]] && break
 sleep 0.1
done
window=$(xdotool search --onlyvisible --class azahar | head -n 1)
xdotool windowmove "$window" 0 0 windowsize "$window" 400 526
xdotool windowfocus "$window"
if [[ -f "$sd/background-ui-auto" ]]; then
 for step in $(seq 1 3); do
  for attempt in $(seq 1 2400); do
   kill -0 "$emulator_pid"
   current=0
   read -r current < "$sd/background-ui-case" || current=0
   [[ "$current" == "$step" ]] && break
   sleep 0.1
  done
  [[ "$current" == "$step" ]]
  sleep 0.4
  import -window root "/emu/ui-$step.png"
 done
 cp "$sd/last-update.log" /emu/update.log
 cp "$sd/background-ui-assertions.csv" /emu/ui-assertions.csv
 grep -Eq '^1,20,[0-9]+,1,0$' /emu/ui-assertions.csv
 grep -Eq '^2,0,[0-9]+,0,0$' /emu/ui-assertions.csv
 grep -Eq '^3,20,12,0,0$' /emu/ui-assertions.csv
 grep -qx 'ignored' "$sd/background-ui-ready-race"
 grep -q 'Update ready\.' /emu/update.log
 printf 'PASS: pause, resume, keep playing and ready UI; no installation.\n' > /emu/result.txt
 exit 0
fi
for step in $(seq 1 24); do
 for attempt in $(seq 1 500); do
  kill -0 "$emulator_pid"
  current=0
  read -r current < "$sd/performance-case" || current=0
  [[ "$current" =~ ^[0-9]+$ ]] || current=0
  ((current>=step)) && break
  sleep 0.1
 done
 [[ "$current" == "$step" ]]
 sleep 1.5
 import -window root "/emu/case-$step.png"
 printf 'Captured case %s.\n' "$step"
done
for attempt in $(seq 1 600); do
 kill -0 "$emulator_pid"
 [[ -s "$sd/performance-done" ]] && break
 sleep 0.1
done
[[ -s "$sd/performance-done" ]]
cp "$sd/performance.csv" "$sd/performance-cases.csv" /emu/
if [[ -f "$sd/background-update-auto" ]]; then
 for attempt in $(seq 1 1800); do
  kill -0 "$emulator_pid"
  grep -q 'Update ready\.' "$sd/last-update.log" && break
  grep -q 'phase=10 ' "$sd/last-update.log" && { cat "$sd/last-update.log"; exit 1; }
  sleep 0.1
 done
 grep -q 'Update ready\.' "$sd/last-update.log"
 cp "$sd/last-update.log" /emu/update.log
 [[ -s "$sd/update.cia.part" || -s /work/out/gopenguin-background-test.3dsx.part ]]
fi
printf 'PASS: 24 ready scenes; isolated native replay, no installation.\n' > /emu/result.txt
