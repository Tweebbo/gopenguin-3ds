#!/usr/bin/env python3
"""Stage a small bootable RomFS and immutable, separately updated SD resources."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--romfs', type=Path, default=Path('romfs'))
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--version', required=True)
parser.add_argument('--base', action='append', default=[], metavar='VERSION=ROMFS')
parser.add_argument('--base-index', action='append', default=[], metavar='VERSION=INDEX')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
output = args.output.resolve()
if root / 'build' not in output.parents or output.exists():
    raise SystemExit('Use a new output directory below build/.')
if len(args.base) + len(args.base_index) > 4:
    raise SystemExit('At most four resource deltas are supported.')
import re
if not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
    raise SystemExit('Invalid release version.')

def files(folder):
    result = {}
    for file in sorted(folder.rglob('*')):
        if file.is_symlink():
            raise ValueError('Resource inputs must not contain symlinks.')
        relative = file.relative_to(folder).as_posix()
        if any(part.startswith('.') for part in file.relative_to(folder).parts):
            continue
        if file.is_file():
            if len(relative.encode()) >= 128 or not re.fullmatch(r'[A-Za-z0-9_./-]+', relative):
                raise ValueError('Unsupported resource path: ' + relative)
            data = file.read_bytes()
            result[relative] = dict(path=relative, bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), source=file)
    return result

def bootstrap(name):
    return (name.startswith('data/') or name == 'town-mask.bin' or
            name in ['scenes/town.swl', 'scenes/town.nav', 'gfx/swl-town-0.t3x'] or
            (name.startswith('gfx/') and not name.startswith('gfx/swl-')))

all_files = files(args.romfs)
resources = dict(sorted((name, entry) for name, entry in all_files.items() if not bootstrap(name)))
if not resources or len(resources) > 4096 or sum(r['bytes'] for r in resources.values()) > 512 * 1024 * 1024:
    raise ValueError('Resource limits exceeded or no resources found.')
(output / 'romfs/data').mkdir(parents=True)
for name, entry in all_files.items():
    if bootstrap(name):
        target = output / 'romfs' / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(entry['source'], target)
index = bytearray(b'GPRIDX1\0' + struct.pack('<II', len(resources), sum(r['bytes'] for r in resources.values())))
for name, entry in resources.items():
    index.extend(name.encode().ljust(128, b'\0') + bytes.fromhex(entry['sha256']) + struct.pack('<I', entry['bytes']))
(output / 'gopenguin-resources.idx').write_bytes(index)
(output / 'romfs/data/resources.idx').write_bytes(index)
unique = {r['sha256']: r for r in resources.values()}

def bundle(name, entries):
    with (output / name).open('wb') as stream:
        stream.write(b'GPRBND1\0' + struct.pack('<I', len(entries)))
        for sha, entry in sorted(entries.items()):
            stream.write(bytes.fromhex(sha) + struct.pack('<I', entry['bytes']))
            with entry['source'].open('rb') as source:
                shutil.copyfileobj(source, stream, 131072)

def artifact(name):
    file = output / name
    with file.open('rb') as stream:
        sha = hashlib.file_digest(stream, 'sha256').hexdigest()
    return dict(url=f'https://github.com/Tweebbo/gopenguin-3ds/releases/download/v{args.version}/{name}',
                bytes=file.stat().st_size, sha256=sha)

bundle('gopenguin-resources.gpb', unique)
offer = dict(schema=1, index=artifact('gopenguin-resources.idx'), bundle=artifact('gopenguin-resources.gpb'), deltas=[])
for base in args.base + args.base_index:
    version, folder = base.split('=', 1)
    if not re.fullmatch(r'\d+\.\d+\.\d+', version) or tuple(map(int, version.split('.'))) >= tuple(map(int, args.version.split('.'))) or any(d['from'] == version for d in offer['deltas']):
        raise ValueError('Invalid or repeated resource base version.')
    if base in args.base_index:
        raw = Path(folder).read_bytes()
        if len(raw) < 16 or raw[:8] != b'GPRIDX1\0':
            raise ValueError('Invalid resource base index.')
        count, total = struct.unpack_from('<II', raw, 8)
        if not 0 < count <= 4096 or total > 512*1024*1024 or len(raw) != 16 + count * 164:
            raise ValueError('Invalid resource base index size.')
        names, size = [], 0
        for n in range(count):
            record = raw[16+n*164:16+(n+1)*164]
            name = record[:128].split(b'\0', 1)[0].decode('ascii')
            if b'\0' not in record[:128] or not re.fullmatch(r'[A-Za-z0-9_./-]+', name) or name.startswith('/') or any(part in ('', '.', '..') for part in name.split('/')) or (names and name <= names[-1]):
                raise ValueError('Invalid resource base path.')
            names.append(name)
            size += struct.unpack_from('<I', record, 160)[0]
        if size != total:
            raise ValueError('Invalid resource base byte total.')
        previous = {raw[16+n*164+128:16+n*164+160].hex() for n in range(count)}
    else:
        previous = {entry['sha256'] for entry in files(Path(folder)).values()}
    changed = {sha: entry for sha, entry in unique.items() if sha not in previous}
    name = f'gopenguin-resources-from-{version}.gpb'
    bundle(name, changed)
    offer['deltas'].append(dict(**{'from': version}, bundle=artifact(name)))
(output / 'resource-offer.json').write_text(json.dumps(offer, indent=2) + '\n')
(output / 'romfs/data/resource-offer.json').write_text(json.dumps(offer, indent=2) + '\n')
report = dict(status='STAGED_NOT_PUBLISHED', version=args.version, files=len(resources), uniqueObjects=len(unique),
              fullRomfsBytes=sum(r['bytes'] for r in all_files.values()), externalBytes=sum(r['bytes'] for r in unique.values()),
              bootstrapBytes=sum(p.stat().st_size for p in (output / 'romfs').rglob('*') if p.is_file()),
              offer=offer)
(output / 'layout.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report))
