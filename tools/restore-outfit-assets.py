#!/usr/bin/env python3
"""Restore the reviewed outfit banks and verify their original hashes."""
import argparse
import hashlib
import json
import os
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--romfs', type=Path, default=root / 'romfs')
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
assets = root / 'assets/outfits'
manifest = json.loads((assets / 'manifest.json').read_text())
restored = 0
for entry in manifest['results']:
    if entry['kind'] not in ('avatars', 'outfits') or not isinstance(entry['id'], int):
        raise ValueError('Invalid outfit resource path')
    relative = Path(entry['kind']) / f"{entry['id']}.gpa"
    data = (assets / 'romfs' / relative).read_bytes()
    if len(data) != entry['bytes'] or hashlib.sha256(data).hexdigest() != entry['sha256']:
        raise ValueError(f'Reviewed outfit resource changed: {relative}')
    target = args.romfs / relative
    if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest() == entry['sha256']:
        continue
    if args.check:
        raise ValueError(f'Packaged outfit resource differs: {relative}')
    if target.exists():
        raise ValueError(f'Refusing to replace an unknown outfit resource: {relative}')
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(data)
    os.replace(temporary, target)
    restored += 1
print(f"Outfit resources: {len(manifest['results'])} verified, {restored} restored.")
