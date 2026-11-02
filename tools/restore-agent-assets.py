#!/usr/bin/env python3
"""Restore the pinned mission 2 banks without replacing unknown resources."""
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
args = parser.parse_args()
assets = root / 'assets/mission2'
profile = json.loads((assets / 'profile.json').read_text())
pending = []
seen = set()
for entry in profile['files']:
    name = entry['path']
    if name in seen or not re.fullmatch(r'missions/2/[A-Za-z0-9]+\.gpk', name):
        raise ValueError('Invalid or repeated mission resource path')
    seen.add(name)
    data = (assets / 'romfs' / name).read_bytes()
    checksum = hashlib.sha256(data).hexdigest()
    if len(data) != entry['bytes'] or checksum != entry['sha256']:
        raise ValueError(f'Pinned mission resource changed: {name}')
    target = args.romfs / name
    if target.exists():
        if hashlib.sha256(target.read_bytes()).hexdigest() != checksum:
            raise ValueError(f'Unknown mission resource; refusing to replace: {name}')
    else:
        pending.append((target, data))
if len(seen) != 12:
    raise ValueError('Expected twelve mission 2 banks')
for target, data in pending:
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(data)
    os.replace(temporary, target)
print(f'Mission 2 profile: {len(seen)} verified files, {len(pending)} restored.')
