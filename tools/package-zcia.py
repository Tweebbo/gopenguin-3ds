#!/usr/bin/env python3
"""Create an optional lossless, small-window Z3DS CIA download and verify every byte."""
import argparse
import ctypes
import ctypes.util
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--frame-kib', type=int, choices=[128, 256], default=256)
parser.add_argument('--level', type=int, choices=range(1, 20), default=9)
parser.add_argument('--report', type=Path)
args = parser.parse_args()
if args.source.resolve() == args.output.resolve() or args.source.suffix.lower() != '.cia' or args.output.suffix.lower() != '.zcia':
    parser.error('Use distinct .cia input and .zcia output paths; the full CIA is retained.')
if args.output.exists():
    parser.error('Output already exists; choose a new artifact path.')
size = args.source.stat().st_size
if not 32 <= size <= 512*1024*1024:
    parser.error('CIA input must be between 32 bytes and 512 MiB.')

library = ctypes.util.find_library('zstd')
if not library:
    parser.error('libzstd is required on the build host.')
zstd = ctypes.CDLL(library)
for name, result, arguments in [
    ('ZSTD_compressBound', ctypes.c_size_t, [ctypes.c_size_t]),
    ('ZSTD_compress', ctypes.c_size_t, [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]),
    ('ZSTD_decompress', ctypes.c_size_t, [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t]),
    ('ZSTD_isError', ctypes.c_uint, [ctypes.c_size_t]),
    ('ZSTD_getFrameContentSize', ctypes.c_ulonglong, [ctypes.c_void_p, ctypes.c_size_t]),
    ('ZSTD_estimateDStreamSize', ctypes.c_size_t, [ctypes.c_size_t]),
]:
    function = getattr(zstd, name);function.restype = result;function.argtypes = arguments

frame_bytes = args.frame_kib*1024
started = time.monotonic()
original = hashlib.sha256()
entries = []
maximum_window = 0
args.output.parent.mkdir(parents=True, exist_ok=True)
temporary = args.output.with_suffix('.zcia.part')
try:
    with args.source.open('rb') as source, temporary.open('w+b') as output:
        output.write(bytes(32))
        while data := source.read(frame_bytes):
            original.update(data)
            capacity = zstd.ZSTD_compressBound(len(data))
            buffer = ctypes.create_string_buffer(capacity)
            stored = zstd.ZSTD_compress(buffer, capacity, data, len(data), args.level)
            if zstd.ZSTD_isError(stored):
                raise ValueError('Zstandard compression failed')
            compressed = buffer.raw[:stored]
            if compressed[4]&0x20:
                window = zstd.ZSTD_getFrameContentSize(compressed, len(compressed))
            else:
                descriptor = compressed[5]
                base = 1 << (10+(descriptor>>3))
                window = base+(base>>3)*(descriptor&7)
            if window > frame_bytes:
                raise ValueError('Compressed frame exceeds the configured decoder window')
            maximum_window = max(maximum_window, window)
            decoded = ctypes.create_string_buffer(len(data))
            actual = zstd.ZSTD_decompress(decoded, len(data), compressed, len(compressed))
            if zstd.ZSTD_isError(actual) or actual != len(data) or decoded.raw != data:
                raise ValueError('Lossless frame verification failed')
            output.write(compressed)
            entries.append((stored, len(data)))
        table = b''.join(struct.pack('<II', *entry) for entry in entries)
        footer = struct.pack('<IBI', len(entries), 0, 0x8F92EAB1)
        output.write(struct.pack('<II', 0x184D2A5E, len(table)+len(footer))+table+footer)
        compressed_bytes = output.tell()-32
        output.seek(0)
        output.write(struct.pack('<4s4sBBHIQQ', b'Z3DS', b'CIA\0', 1, 0, 32, 0, compressed_bytes, size))
    # Use the separate zstd executable to verify the complete concatenated frame stream.
    decoded_sha = hashlib.sha256()
    count = 0
    with temporary.open('rb') as packed:
        packed.seek(32)
        process = subprocess.Popen(['zstd', '-d', '-q', '-c'], stdin=packed, stdout=subprocess.PIPE)
        while chunk := process.stdout.read(131072):
            decoded_sha.update(chunk);count += len(chunk)
        if process.wait() or count != size or decoded_sha.digest() != original.digest():
            raise ValueError('Independent whole-file round trip failed')
    temporary.rename(args.output)
except BaseException:
    temporary.unlink(missing_ok=True)
    raise

with args.output.open('rb') as packed:
    compressed_sha = hashlib.file_digest(packed, 'sha256').hexdigest()
decoder_bytes = zstd.ZSTD_estimateDStreamSize(frame_bytes)
if zstd.ZSTD_isError(decoder_bytes):
    raise ValueError('Could not estimate the bounded streaming decoder')
report = dict(status='PASS', source=str(args.source), output=str(args.output), originalBytes=size,
              compressedBytes=args.output.stat().st_size, savedBytes=size-args.output.stat().st_size,
              originalSha256=original.hexdigest(), compressedSha256=compressed_sha,
              decodedSha256=decoded_sha.hexdigest(), frameBytes=frame_bytes, maxWindowBytes=maximum_window, frames=len(entries),
              decoderStreamEstimateBytes=decoder_bytes, level=args.level,
              hostSeconds=round(time.monotonic()-started, 3), texturesChanged=False,
              compatibility='Standard Z3DS v1 CIA wrapper and seekable Zstandard; optional 3dsDL/Azahar download. The in-game updater still uses its existing CIA/patch path.',
              physicalConsoleMeasured=False)
if args.report:
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report))
