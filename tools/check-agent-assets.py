#!/usr/bin/env python3
"""Reject incomplete mission 2 resources before compiling a native candidate."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--romfs', type=Path, default=Path('romfs'))
args = parser.parse_args()
rooms = ['sportShop','mountain','hillSigns','miniGame','crashSite','clearing1','clearing2','puffles','river','outsideCave','cave','guide']
for room in rooms:
    file = args.romfs / 'missions/2' / (room + '.gpk')
    if not file.exists():
        raise SystemExit(f'{file}: missing. Generate mission resources with tools/export-agent2.mjs; see docs/DEVELOPING.md.')
    data = file.read_bytes()
    if len(data) < 80 or data[:4] != b'GPK1' or struct.unpack_from('<I', data, 8)[0] != len(data) or hashlib.sha256(data[64:]).digest() != data[32:64]:
        raise SystemExit(f'{file}: invalid movie package')
    count = struct.unpack_from('<I', data, 12)[0]
    if not 3 <= count <= 8:
        raise SystemExit(f'{file}: invalid texture page count')
    offset, size = struct.unpack_from('<II', data, 72)
    metadata = json.loads(data[offset:offset+size])
    if metadata.get('mission') != 2 or metadata.get('room') != room:
        raise SystemExit(f'{file}: wrong scene metadata')
    textures = 0
    for part in range(2, count):
        start, length = struct.unpack_from('<II', data, 64+part*8)
        if length < 4 or start+length > len(data):
            raise SystemExit(f'{file}: invalid texture bounds')
        fmt = data[start+3]
        textures += 1024*512*(4 if fmt == 0 else 3 if fmt == 1 else 2 if fmt <= 5 else 1)
    if textures > 6*1024*1024:
        raise SystemExit(f'{file}: textures exceed the 6 MiB scene budget')
print(f'Agent resources: {len(rooms)} verified mission 2 banks.')
