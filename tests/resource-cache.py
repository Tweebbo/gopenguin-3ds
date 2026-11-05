#!/usr/bin/env python3
"""Measure resource verification work and exercise cache invalidation and repair."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / ('build/resource-cache-sanitizer-tests' if os.environ.get('SANITIZE') else 'build/resource-cache-tests')
OUT.mkdir(parents=True, exist_ok=True)
binary = OUT / 'store'
flags = ['-std=c11', '-D_POSIX_C_SOURCE=200809L', '-O2', '-Wall', '-Wextra', '-Werror', '-Isource', '-Ibuild/host-include']
if os.environ.get('SANITIZE'):
    flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
subprocess.run(['cc', *flags, 'tests/resource-store.c', 'source/resource_store.c', 'source/hash.c', '-lcrypto', '-o', str(binary)], cwd=ROOT, check=True)


def index(entries):
    data = b'GPRIDX1\0' + struct.pack('<II', len(entries), sum(map(len, entries.values())))
    return data + b''.join(name.encode().ljust(128, b'\0') + hashlib.sha256(value).digest() + struct.pack('<I', len(value)) for name, value in sorted(entries.items()))


def run(mode, manifest, store, source):
    started = time.monotonic()
    result = subprocess.run([str(binary), mode, str(manifest), str(store), str(source)], capture_output=True, text=True, check=True)
    return dict(json.loads(result.stdout), hostSeconds=round(time.monotonic()-started, 4))


checks = []
with tempfile.TemporaryDirectory(dir=OUT) as temporary:
    folder = Path(temporary)
    source = folder/'source';source.mkdir()
    contents = {'asset.bin': bytes(range(256))*4096, 'second.bin': b'unchanged original texture'*32768}
    for name, value in contents.items():
        (source/name).write_bytes(value)
    manifest = folder/'resources.idx';manifest.write_bytes(index(contents))
    store = folder/'objects'
    total = sum(map(len, contents.values()))
    first = run('cached', manifest, store, source)
    assert first['ready'] and first['copied'] == first['pacedBytes'] == total
    checks.append(dict(case='first migration hashes copied bytes once', **first))
    repeated = run('cached', manifest, store, source)
    assert repeated['ready'] and repeated['cached'] == total and repeated['pacedBytes'] == 0
    checks.append(dict(case='unchanged verified objects require zero content reads', **repeated))
    strict = run('reuse', manifest, store, source)
    assert strict['verified'] == strict['pacedBytes'] == total and strict['cached'] == 0
    checks.append(dict(case='explicit repair bypasses all cached receipts', **strict))
    digest = hashlib.sha256(contents['asset.bin']).hexdigest()
    target = store/digest[0]/(digest+'.bin')
    previous = target.stat()
    target.write_bytes(b'X'*len(contents['asset.bin']))
    os.utime(target, ns=(previous.st_atime_ns, previous.st_mtime_ns))
    repaired = run('cached', manifest, store, source)
    assert target.read_bytes() == contents['asset.bin'] and repaired['copied'] == len(contents['asset.bin'])
    checks.append(dict(case='same-size modified object invalidates receipt even with restored mtime on host', **repaired))
    receipt = Path(str(target)+'.verified');receipt.write_bytes(b'broken receipt')
    missing_receipt = run('cached', manifest, store, source)
    assert missing_receipt['verified'] == len(contents['asset.bin'])
    checks.append(dict(case='damaged receipt requires full object verification', **missing_receipt))
    target.write_bytes(b'X'*len(contents['asset.bin']))
    unavailable = run('reuse', manifest, store, folder/'absent')
    assert not unavailable['ready'] and not target.exists()
    checks.append(dict(case='repair removes damaged object so bundle fallback cannot mistake it for ready', **unavailable))
    changed = dict(contents);changed['asset.bin'] = b'new expected resource'
    manifest.write_bytes(index(changed));(source/'asset.bin').write_bytes(changed['asset.bin'])
    new = run('cached', manifest, store, source)
    assert new['ready'] and new['copied'] == len(changed['asset.bin'])
    checks.append(dict(case='changed index SHA selects a different immutable object', **new))

real_results = []
real = ROOT/'build/package-igloo-bridge'
snapshots = sorted(real.glob('inputs-*/romfs'))
if snapshots and not os.environ.get('SANITIZE'):
    with tempfile.TemporaryDirectory(dir=OUT) as temporary:
        store = Path(temporary)/'objects'
        manifest = real/'gopenguin-resources.idx'
        first = run('cached', manifest, store, snapshots[-1])
        assert first['ready']
        again = run('cached', manifest, store, snapshots[-1])
        assert again['ready'] and again['cached'] == first['copied']+first['existing'] and again['pacedBytes'] == 0, (first, again)
        repair = run('reuse', manifest, store, snapshots[-1])
        assert repair['ready'] and repair['pacedBytes'] == first['copied']+first['existing']
        real_results = [dict(case=name, **value) for name, value in [('real first migration', first), ('real cached update', again), ('real explicit repair', repair)]]

report = dict(status='PASS', checks=checks, realResources=real_results, receiptBytes=72, newCacheHeapBuffers=0, ioBufferBytes=131072, texturesChanged=False, physicalConsoleMeasured=False,
              limitation='Normal reuse trusts an earlier SHA-verified object when expected hash, size and available filesystem timestamps are unchanged. Silent SD corruption without a metadata change requires explicit checksum repair. Host nanosecond timestamps are stronger than 3DS timestamp granularity.')
(OUT/('sanitizers.json' if os.environ.get('SANITIZE') else 'report.json')).write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report))
