#!/usr/bin/env bash
set -euo pipefail
export DISPLAY=:99 QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2
if [[ "$IGLOO_FAMILY" == new ]];then export DISPLAY=:100;fi
export XDG_CONFIG_HOME=/emu/home/.config XDG_DATA_HOME=/emu/home/.local/share
sd=/emu/home/.local/share/azahar-emu/sdmc/3ds/gopenguin
Xvfb "$DISPLAY" -screen 0 1280x1024x24 > /emu/xvfb.log 2>&1 &
sleep 2
/azahar/AppRun /emu/app.3dsx > /emu/azahar.log 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true' EXIT
count(){ grep -cE "$1" "$sd/test-state.log" 2>/dev/null || true; }
wait_count(){
 for attempt in $(seq 1 400); do
  kill -0 "$pid"
  if [[ $(count "$1") -ge $2 ]]; then return; fi
  sleep 0.1
 done
 import -window root /emu/timeout.png
 cp "$sd/test-state.log" /emu/protocol.log
 return 1
}
mode(){
 exec 9<>/dev/tcp/127.0.0.1/24108
 printf 'GET /%s/%s HTTP/1.0\r\nHost: localhost\r\n\r\n' "$IGLOO_FAMILY" "$1" >&9
 cat <&9 > /dev/null
 exec 9>&-
}
step(){ printf '%s\n' "$1" > "$sd/gameplay-step"; }
capture(){ sleep 1; import -window root "/emu/$1.png"; }
back(){ local before; before=$(count '^jr .*first=100 ');step 12;wait_count '^jr .*first=100 ' "$((before+1))";sleep 0.5; }
wait_count '^jr .*first=100 ' 1
window=$(xdotool search --onlyvisible --class azahar | head -n 1)
xdotool windowmove "$window" 0 0 windowsize "$window" 400 526
xdotool windowfocus "$window"
mode normal
sleep 1.5
for attempt in $(seq 1 5);do
 xdotool mousemove 320 445 mousedown 1
 sleep 0.3
 xdotool mouseup 1
 sleep 1
 if [[ $(count '^gm args=') -ge 1 ]];then break;fi
done
wait_count '^igloo entered ' 1;capture normal
step 13;wait_count '^igloo entered ' 2;capture reuse
back
mode drop
step 11
for tap in $(seq 20 24);do sleep 0.3;step "$tap";done
wait_count '^igloo entered ' 3;capture drop
back
mode delay
step 13
for tap in $(seq 25 32);do sleep 0.35;step "$tap";done
wait_count '^igloo entered ' 4;capture delay
sleep 3
back
mode custom
step 14;sleep 1;step 11
wait_count '^igloo entered ' 5;capture custom
step 13;wait_count '^igloo entered ' 6;capture custom-reuse
back
mode normal
step 10;wait_count '^game command=psa' 1;capture mission
step 11;wait_count '^igloo entered ' 7;capture mission-return
back
mode timeout
step 13
for tap in $(seq 20 29);do sleep 0.35;step "$tap";done
wait_count '^igloo stopped action=3' 1;capture no-reply
[[ $(count '^igloo entered ') -eq 7 ]]
mode normal
step 11;wait_count '^igloo entered ' 8;capture recovery
back
step 15;wait_count '^catalogue id=' 1;capture catalogue
step 13;wait_count '^igloo entered ' 9
wait_count '^catalogue closed id=' 1;capture catalogue-return
cp "$sd/test-state.log" /emu/protocol.log
printf 'PASS: normal room touch, warm bank, missing/delayed replies, repeated taps, full asset queue, mission return, timeout, recovery and catalogue closure.\n' > /emu/result.txt
