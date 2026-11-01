#!/usr/bin/env python3
"""Fail a release when the RomFS cannot satisfy what the 3DS client loads at boot.

A client whose rooms or boot textures are missing stops at "Could not load demo
assets" before it reaches the updater, so it cannot repair itself.
"""
import re
import sys
import subprocess
from pathlib import Path

root = Path(sys.argv[1] if len(sys.argv) > 1 else '.')
romfs = root / 'romfs'
main = (root / 'source/main.c').read_text()
rooms = re.findall(r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*"([a-z0-9-]+)"\s*,', (root / 'source/island_rooms.h').read_text())
if not rooms:
    sys.exit('No rooms were found in source/island_rooms.h.')

needed = {'data/client.bin', 'data/shell-text.json'}
boot = re.search(r'bool ok=clientData&&gpuOK(.*?);', main)
if not boot:
    sys.exit('The boot asset list was not found in source/main.c.')
needed |= {f'gfx/{name}.t3x' for name in re.findall(r'load\(&\w+,"([^"]+)"\)', boot.group(1))}
pages = re.search(r'define PENGUIN_PAGES\s+(\d+)', (root / 'source/penguin_atlas.h').read_text())
if not pages:
    sys.exit('PENGUIN_PAGES was not found in source/penguin_atlas.h.')
for page in range(int(pages.group(1))):
    needed |= {f'gfx/penguin-body-{page}.t3x', f'gfx/penguin-detail-{page}.t3x'}
for _, count, key in rooms:
    needed |= {f'scenes/{key}.nav', f'scenes/{key}.swl'}
    needed |= {f'gfx/swl-{key}-{page}.t3x' for page in range(int(count))}

missing = sorted(path for path in needed if not (romfs / path).is_file())
if missing:
    sys.exit('RomFS does not match the client source. Missing:\n  ' + '\n  '.join(missing))
print(f'RomFS provides all {len(needed)} boot and room files for {len(rooms)} rooms.')
subprocess.run([sys.executable, str(root / 'tools/restore-seasonal-assets.py'),
                '--romfs', str(romfs), '--check'], check=True)
