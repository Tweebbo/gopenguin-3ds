#!/usr/bin/env python3
"""Restore reviewed resources over the release RomFS base."""
import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--romfs', type=Path, default=root / 'romfs')
parser.add_argument('--skip-missing', action='store_true')
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
if args.skip_missing and not (args.romfs / 'data/client.bin').exists():
    raise SystemExit(0)
assets = root / 'assets/hud-icons'
profile = json.loads((assets / 'profile.json').read_text())
pending = []
for entry in profile['files']:
    relative = Path(entry['path'])
    if relative.is_absolute() or '..' in relative.parts:
        raise ValueError('Invalid resource path')
    source = assets / 'romfs' / relative
    data = source.read_bytes()
    if len(data) != entry['bytes'] or hashlib.sha256(data).hexdigest() != entry['sha256']:
        raise ValueError(f'Reviewed resource changed: {relative}')
    target = args.romfs / relative
    current = hashlib.sha256(target.read_bytes()).hexdigest() if target.exists() else None
    if current == entry['sha256']:
        continue
    if args.check:
        raise ValueError(f'Release resource does not match the reviewed profile: {relative}')
    if current not in entry['replacesSha256'] and not (current is None and entry['allowMissing']):
        raise ValueError(f'Unknown base resource; refusing to replace: {relative}')
    pending.append((target, data))

for target, data in pending:
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(data)
    os.replace(temporary, target)
print(f"Resource profile: {len(profile['files'])} verified files, {len(pending)} restored.")
subprocess.run([sys.executable, str(root / 'tools/restore-outfit-assets.py'),
                '--romfs', str(args.romfs), *(['--check'] if args.check else [])], check=True)
