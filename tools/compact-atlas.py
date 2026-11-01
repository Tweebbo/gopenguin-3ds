#!/usr/bin/env python3
"""Rebuild one verified RGBA8 atlas as RGBA4 without changing its layout."""
import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--romfs', type=Path, default=root / 'romfs')
parser.add_argument('--skip-missing', action='store_true')
args = parser.parse_args()
profile = json.loads((root / 'assets/performance/atlas-profile.json').read_text())
target = args.romfs / 'gfx' / (profile['atlas'] + '.t3x')
if args.skip_missing and not target.exists():
    raise SystemExit(0)


def checksum(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


source = root / profile['source']
if checksum(source) != profile['sourceSha256']:
    raise ValueError('Atlas source changed; review and update its provenance first')
current = checksum(target)
if current == profile['targetTextureSha256']:
    raise SystemExit(0)
if current != profile['originalTextureSha256']:
    raise ValueError('Atlas differs from the reviewed base; refusing to replace it')

(root / 'build').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='atlas-profile-', dir=root / 'build') as temp:
    reference = Path(temp) / 'reference.t3x'
    compact = Path(temp) / 'compact.t3x'
    subprocess.run(['tex3ds', '-f', 'rgba8888', '-z', 'auto', '-o', str(reference), str(source)], check=True)
    if checksum(reference) != current:
        raise ValueError('The source PNG does not reproduce the original atlas with this SDK')
    subprocess.run(['tex3ds', '-f', 'rgba4', '-z', 'auto', '-o', str(compact), str(source)], check=True)
    old, new = reference.read_bytes(), compact.read_bytes()
    if old[:3] != new[:3] or old[4:17] != new[4:17] or new[3] != 4:
        raise ValueError('Atlas dimensions or subtexture coordinates changed')
    if checksum(compact) != profile['targetTextureSha256']:
        raise ValueError('Compact atlas differs from the reviewed output')
    os.replace(compact, target)
print(f"Atlas {profile['atlas']}: 4 MiB RGBA8 -> 2 MiB RGBA4; layout unchanged.")
