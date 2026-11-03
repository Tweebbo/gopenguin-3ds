#!/usr/bin/env bash
set -euo pipefail
export DISPLAY=:99 QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2
export XDG_CONFIG_HOME=/emu/home/.config XDG_DATA_HOME=/emu/home/.local/share
sd=/emu/home/.local/share/azahar-emu/sdmc/3ds/gopenguin
Xvfb :99 -screen 0 1280x1024x24 > /emu/xvfb.log 2>&1 &
sleep 2
/azahar/AppRun "$IGLOO_APP" > /emu/azahar.log 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true' EXIT
wait_for(){
 for attempt in $(seq 1 600); do
  kill -0 "$pid"
  if [[ -f "$sd/test-state.log" ]] && grep -Eq "$1" "$sd/test-state.log"; then return; fi
  sleep 0.1
 done
 import -window root /emu/timeout.png
 return 1
}
wait_for '^jr .*first=100 '
window=$(xdotool search --onlyvisible --class azahar | head -n 1)
xdotool windowmove "$window" 0 0 windowsize "$window" 400 526
xdotool windowfocus "$window"
printf '10\n' > "$sd/gameplay-step"
wait_for '^game command=psa'
sleep 2
import -window root /emu/mission.png
printf '11\n' > "$sd/gameplay-step"
wait_for '^igloo applied=1'
sleep 4
import -window root /emu/igloo.png
cp "$sd/test-state.log" /emu/protocol.log
printf 'Native igloo transition captured against localhost only.\n' > /emu/result.txt
