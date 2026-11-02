#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/minigame-render
flags=(-O2 -std=c11 -D_POSIX_C_SOURCE=200809L -DGAME_THINICE_READY -Wall -Wextra -Werror -Wno-misleading-indentation -Isource)
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-g -fsanitize=address,undefined -fno-omit-frame-pointer); fi
bank=(tests/game-bank-host.c source/package.c source/hash.c source/movie_meta.c source/swl.c)
for game in cart hydro roundup thinice sled; do
  define="TEST_${game^^}"
  for mode in eager deferred; do
    extra=()
    if [[ "$mode" == eager ]]; then extra+=(-DEAGER_REFERENCE); fi
    cc "${flags[@]}" "${extra[@]}" -D"$define" tests/minigame-render.c source/game_port.c "source/game_${game}_model.c" tests/game-controller-stubs.c "${bank[@]}" -lcrypto -lz -ljansson -lm -o "build/minigame-render/$game-$mode"
    "build/minigame-render/$game-$mode" > "build/minigame-render/$game-$mode.txt"
  done
done
python3 - <<'PY'
import json
from pathlib import Path
root=Path('build/minigame-render')
results=[]
for game in ['cart','hydro','roundup','thinice','sled']:
    eager=(root/f'{game}-eager.txt').read_text().split()
    deferred=(root/f'{game}-deferred.txt').read_text().split()
    assert eager[1]==deferred[1],f'{game}: rendered art differs'
    assert int(deferred[2])<int(eager[2]),f'{game}: no preparation reduction'
    results.append(dict(game=game,frames=int(eager[3]),renderHash=eager[1],eagerPreparations=int(eager[2]),deferredPreparations=int(deferred[2])))
(root/'results.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
PY
