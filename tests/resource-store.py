#!/usr/bin/env python3
"""Exercise SD migration, content binding, repair and malformed resource bundles."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parent.parent
build = root / 'build/resource-tests'
build.mkdir(parents=True, exist_ok=True)
binary = build / 'store'
flags = ['-std=c11', '-D_POSIX_C_SOURCE=200809L', '-O2', '-Wall', '-Wextra', '-Werror', '-Isource', '-Ibuild/host-include']
if os.environ.get('SANITIZE'):
    flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
subprocess.run(['cc', *flags, 'tests/resource-store.c', 'source/resource_store.c', 'source/hash.c',
                '-lcrypto', '-o', str(binary)], cwd=root, check=True)
sha = lambda b: hashlib.sha256(b).hexdigest()
checks = []

def index(entries):
    output = b'GPRIDX1\0' + struct.pack('<II', len(entries), sum(len(data) for data in entries.values()))
    for name, data in sorted(entries.items()):
        output += name.encode().ljust(128, b'\0') + hashlib.sha256(data).digest() + struct.pack('<I', len(data))
    return output

def bundle(entries):
    unique = {sha(data): data for data in entries.values()}
    return b'GPRBND1\0' + struct.pack('<I', len(unique)) + b''.join(bytes.fromhex(key) + struct.pack('<I', len(data)) + data for key, data in sorted(unique.items()))

def run(*args, valid=True, cwd=root):
    result = subprocess.run([str(binary), *map(str, args)], cwd=cwd, text=True, capture_output=True)
    assert (result.returncode == 0) == valid, (args, result.returncode, result.stdout, result.stderr)
    return result.stdout

with tempfile.TemporaryDirectory(dir=build) as temporary:
    folder = Path(temporary)
    source = folder / 'embedded'
    (source / 'audio').mkdir(parents=True)
    (source / 'games').mkdir()
    contents = {'audio/song.gpz': b'music' * 30000, 'games/cart.gpk': b'original-game', 'games/empty.gpk': b''}
    for name, data in contents.items():
        (source / name).write_bytes(data)
    old = folder / 'old.idx';old.write_bytes(index(contents))
    store = folder / 'objects'
    stats = json.loads(run('reuse', old, store, source))
    assert stats['ready'] and stats['copied'] == sum(map(len, contents.values()))
    checks.append('Bridge migration copies and reads back every original resource')
    stats = json.loads(run('reuse', old, store, source))
    assert stats['ready'] and not stats['copied'] and stats['existing'] == sum(map(len, contents.values()))
    checks.append('Repeated updates reuse immutable SD objects without rewriting them')
    new_contents = dict(contents, **{'games/cart.gpk': b'updated-game'})
    target = folder / 'new.idx';target.write_bytes(index(new_contents))
    run('reuse', target, store, 'romfs:/', old)
    patch = folder / 'delta.gpb';patch.write_bytes(bundle({'games/cart.gpk': new_contents['games/cart.gpk']}))
    stats = json.loads(run('bundle', target, store, patch, sha(patch.read_bytes()), patch.stat().st_size))
    assert stats['ready'] and stats['written'] == len(b'updated-game')
    out = folder / 'read.bin'
    for manifest, expected in [(old, b'original-game'), (target, b'updated-game')]:
        run('open', manifest, store, 'romfs:/games/cart.gpk', out)
        assert out.read_bytes() == expected
    checks.append('Old and new applications resolve their own exact hashes; rollback objects are retained')
    digest = sha(b'updated-game');damaged = store / digest[0] / (digest + '.bin');damaged.write_bytes(b'x' * len(b'updated-game'))
    stats = json.loads(run('reuse', target, store, folder / 'missing'))
    assert stats['files'] < len(new_contents)
    run('bundle', target, store, patch, sha(patch.read_bytes()), patch.stat().st_size)
    assert damaged.read_bytes() == b'updated-game'
    checks.append('Same-size damaged SD objects are detected and repaired')
    raw = patch.read_bytes()
    for label, data, expected_sha in [
        ('truncated', raw[:-1], sha(raw)), ('trailing', raw+b'junk', sha(raw+b'junk')),
        ('wrong digest', raw, '0'*64), ('unknown object', bundle({'evil': b'unknown'}), None),
        ('duplicate object', raw[:8]+struct.pack('<I', 2)+raw[12:]+raw[12:], None),
        ('oversized count', raw[:8]+struct.pack('<I', 5000)+raw[12:], None),
    ]:
        bad = folder / 'bad.gpb';bad.write_bytes(data)
        run('bundle', target, store, bad, expected_sha or sha(data), len(data), valid=False)
        assert not list(store.rglob('*.part'))
        checks.append('Rejects '+label+' bundles without a partial object')
    for label, data in [('traversal', index({'../settings.json': b'bad'})),
                        ('duplicate path', index({'a': b'a'})[:8]+struct.pack('<II',2,2)+index({'a':b'a'})[16:]*2),
                        ('truncated index', target.read_bytes()[:-1]),
                        ('oversized index', b'GPRIDX1\0'+struct.pack('<II',5000,0))]:
        bad = folder / 'bad.idx';bad.write_bytes(data);run('index', bad, valid=False)
        checks.append('Rejects '+label)

real = root / 'build/split-139'
timings = []
if real.exists() and not os.environ.get('SANITIZE'):
    with tempfile.TemporaryDirectory(dir=build) as temporary:
        start = time.monotonic()
        stats = json.loads(run('reuse', real/'gopenguin-resources.idx', Path(temporary)/'objects', root/'romfs'))
        assert stats['ready'] and stats['files'] == 1275
        timings.append(dict(case='real bridge migration', seconds=time.monotonic()-start, **stats))
        start = time.monotonic()
        again = json.loads(run('reuse', real/'gopenguin-resources.idx', Path(temporary)/'objects', root/'romfs'))
        assert again['ready'] and not again['copied']
        timings.append(dict(case='real unchanged resource set', seconds=time.monotonic()-start, **again))
        payload = real / 'gopenguin-resources.gpb'
        with payload.open('rb') as stream:
            checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
        fresh = json.loads(run('bundle', real/'gopenguin-resources.idx', Path(temporary)/'fresh', payload, checksum, payload.stat().st_size))
        assert fresh['ready'] and fresh['files'] == 1263
    checks.append('All 1,275 real resource paths migrate and repeat with zero rewrites')
report = dict(status='PASS', checks=checks, hostTimings=timings, physicalConsoleMeasured=False)
(build / ('sanitizers.json' if os.environ.get('SANITIZE') else 'report.json')).write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report))
