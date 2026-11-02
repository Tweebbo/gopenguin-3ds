#!/usr/bin/env python3
"""Restore the reviewed room and minigame art over its known RomFS base."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--romfs', type=Path, default=root / 'romfs')
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
assets = root / 'assets/sharpness'
profile = json.loads((assets / 'profile.json').read_text())
pending = []
seen = set()
for entry in profile['files']:
    name = entry['path']
    if name in seen or not re.fullmatch(r'(?:gfx/swl-[a-z0-9-]+-\d+\.t3x|scenes/[a-z0-9-]+\.swl|games/[a-z0-9-]+\.gpk)', name):
        raise ValueError('Invalid or repeated sharpness resource path')
    seen.add(name)
    data = (assets / 'romfs' / name).read_bytes()
    checksum = hashlib.sha256(data).hexdigest()
    if len(data) != entry['bytes'] or checksum != entry['sha256']:
        raise ValueError(f'Reviewed sharpness resource changed: {name}')
    target = args.romfs / name
    current = hashlib.sha256(target.read_bytes()).hexdigest() if target.exists() else None
    if current == checksum:
        continue
    if args.check:
        raise ValueError(f'Packaged sharpness resource differs: {name}')
    if current != entry['replacesSha256']:
        raise ValueError(f'Unknown base resource; refusing to replace: {name}')
    pending.append((target, data))

for target, data in pending:
    with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(data)
    os.replace(temporary, target)
print(f'Sharpness profile: {len(seen)} verified files, {len(pending)} restored.')
