#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/old3ds-optimization
cc -O2 -std=c11 -Wall -Wextra -Werror tests/old3ds-workload.c source/demo.c source/actor_index.c source/render_order.c -lm -o build/old3ds-optimization/workload-test
build/old3ds-optimization/workload-test
cc -O2 -std=c11 -Wall -Wextra -Werror tests/name-tags.c source/name_tag.c -o build/old3ds-optimization/name-tags-test
build/old3ds-optimization/name-tags-test
cc -O2 -std=c11 -Wall -Wextra -Werror tests/core.c source/demo.c source/actor_index.c -lm -o build/old3ds-optimization/core-test
build/old3ds-optimization/core-test
