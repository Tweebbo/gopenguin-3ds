#!/usr/bin/env python3
"""Stage lossless GPK2/audio compression; leave source resources unchanged."""
import argparse
import ctypes
import ctypes.util
import hashlib
import json
from pathlib import Path
import struct
import time
import zlib

LIMIT = 24 * 1024 * 1024
sha = lambda data: hashlib.sha256(data).hexdigest()


class Compressor:
    def __init__(self):
        self.lib = None
        name = ctypes.util.find_library('deflate')
        if name:
            lib = ctypes.CDLL(name)
            lib.libdeflate_alloc_compressor.argtypes = [ctypes.c_int]
            lib.libdeflate_alloc_compressor.restype = ctypes.c_void_p
            lib.libdeflate_zlib_compress_bound.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
            lib.libdeflate_zlib_compress_bound.restype = ctypes.c_size_t
            lib.libdeflate_zlib_compress.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t]
            lib.libdeflate_zlib_compress.restype = ctypes.c_size_t
            lib.libdeflate_free_compressor.argtypes = [ctypes.c_void_p]
            self.context = lib.libdeflate_alloc_compressor(12)
            if not self.context:
                raise MemoryError('Could not allocate build compressor')
            self.lib = lib

    def compress(self, data):
        best = zlib.compress(data, 9)
        if self.lib:
            bound = self.lib.libdeflate_zlib_compress_bound(self.context, len(data))
            output = ctypes.create_string_buffer(bound)
            size = self.lib.libdeflate_zlib_compress(self.context, data, len(data), output, bound)
            if size and size < len(best):
                best = output.raw[:size]
        assert zlib.decompress(best) == data
        return best

    def close(self):
        if self.lib:
            self.lib.libdeflate_free_compressor(self.context)


def inflate(data, size):
    stream = zlib.decompressobj()
    decoded = stream.decompress(data, size + 1)
    if len(decoded) != size or not stream.eof or stream.unused_data or stream.unconsumed_tail:
        raise ValueError('Invalid compressed resource length')
    return decoded


def gpk_parts(data):
    if len(data) < 80 or data[:4] not in (b'GPK1', b'GPK2'):
        raise ValueError('Invalid movie package')
    packed = data[:4] == b'GPK2'
    version, total, count = struct.unpack_from('<III', data, 4)
    if version != (2 if packed else 1) or total != len(data) or not 2 <= count <= 13 or hashlib.sha256(data[64:]).digest() != data[32:64]:
        raise ValueError('Invalid movie header/hash')
    expected, result, decoded = 64 + count * (16 if packed else 8), [], 0
    for i in range(count):
        fields = struct.unpack_from('<IIII' if packed else '<II', data, 64 + i * (16 if packed else 8))
        offset, stored = fields[:2]
        size, codec = fields[2:] if packed else (stored, 0)
        if offset != expected or not stored or offset + stored > total or not size or size > LIMIT - decoded or codec not in (0, 1) or (codec == 0 and size != stored):
            raise ValueError('Invalid movie part')
        part = data[offset:offset + stored]
        result.append(inflate(part, size) if codec else part)
        expected += stored
        decoded += size
    if expected != total:
        raise ValueError('Trailing movie bytes')
    return result


def movie(data, compressor):
    parts = gpk_parts(data)
    table, payload, offset = bytearray(), [], 64 + len(parts) * 16
    for part in parts:
        encoded = compressor.compress(part)
        codec = int(len(encoded) < len(part))
        stored = encoded if codec else part
        table += struct.pack('<IIII', offset, len(stored), len(part), codec)
        payload.append(stored)
        offset += len(stored)
    body = bytes(table) + b''.join(payload)
    header = bytearray(data[:64])
    header[:4] = b'GPK2'
    struct.pack_into('<III', header, 4, 2, offset, len(parts))
    header[32:64] = hashlib.sha256(body).digest()
    result = bytes(header) + body
    assert gpk_parts(result) == parts
    return result if len(result) < len(data) else data


def audio(data, compressor):
    if data[:4] != b'GPAZ' or len(data) < 32:
        raise ValueError('Invalid compressed PCM')
    version, rate, decoded, block_size, count, a, b = struct.unpack_from('<7I', data, 4)
    if version != 1 or rate != 22050 or block_size != 8192 or count != (decoded + 8191) // 8192 or a or b:
        raise ValueError('Invalid PCM header')
    offset, expected, table, payload = 32 + count * 8, 32 + count * 8, bytearray(), []
    for i in range(count):
        source, field = struct.unpack_from('<II', data, 32 + i * 8)
        flags, size = field & 0xc0000000, field & 0x3fffffff
        raw_size = min(8192, decoded - i * 8192)
        if source != expected or not size or size > raw_size or source + size > len(data) or flags == 0xc0000000:
            raise ValueError('Invalid PCM block')
        old = data[source:source + size]
        if flags == 0x80000000:
            if len(old) != raw_size:
                raise ValueError('Invalid raw PCM block')
            raw, compressed_flags = old, 0
        else:
            raw, compressed_flags = inflate(old, raw_size), flags
        candidate = compressor.compress(raw)
        if len(candidate) < len(old):
            stored, new_flags = candidate, compressed_flags
        else:
            stored, new_flags = old, flags
        assert (stored if new_flags == 0x80000000 else inflate(stored, raw_size)) == raw
        table += struct.pack('<II', offset, len(stored) | new_flags)
        payload.append(stored)
        offset += len(stored)
        expected += size
    if expected != len(data):
        raise ValueError('Trailing PCM bytes')
    return data[:32] + bytes(table) + b''.join(payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    if source == output or source in output.parents or output in source.parents or output.exists():
        parser.error('Output must be a new directory outside the source tree')
    if (source / 'data/resources.idx').exists():
        parser.error('Optimize before generating a split resource index')
    started, rows, compressor = time.monotonic(), [], Compressor()
    try:
        for file in sorted(source.rglob('*')):
            if not file.is_file():
                continue
            relative = file.relative_to(source)
            if any(part.startswith('.') for part in relative.parts):
                continue
            original = file.read_bytes()
            result = movie(original, compressor) if file.suffix == '.gpk' else audio(original, compressor) if file.suffix == '.gpz' else original
            assert len(result) <= len(original)
            destination = output / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(result)
            rows.append(dict(file=str(relative), sourceBytes=len(original), bytes=len(result), sourceSha256=sha(original), sha256=sha(result)))
    finally:
        compressor.close()
    report = dict(seconds=round(time.monotonic() - started, 2), sourceBytes=sum(r['sourceBytes'] for r in rows), bytes=sum(r['bytes'] for r in rows), changed=sum(r['sha256'] != r['sourceSha256'] for r in rows), codec='zlib-9 + libdeflate-12' if compressor.lib else 'zlib-9', decodedPartsIdentical=True, files=rows)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k:v for k,v in report.items() if k != 'files'}))


if __name__ == '__main__':
    main()
