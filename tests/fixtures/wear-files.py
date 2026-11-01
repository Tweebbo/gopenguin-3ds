import hashlib
import struct
import sys
import os
from pathlib import Path

root = Path(sys.argv[1])
(root / 'romfs:/clothing').mkdir(parents=True)
(root / 'assets').mkdir()


def texture(value, width=8):
    exponent = width.bit_length() - 4
    header = struct.pack('<HBBB6H', 1, exponent | exponent << 3, 0, 0, width, width, 0, 1024, 1024, 0)
    pixels = bytes([value]) * (width * width * 4)
    return header + struct.pack('<I', len(pixels) << 8) + pixels


def seal(header, payload):
    struct.pack_into('<I', header, 8, len(header) + len(payload))
    header[32:64] = hashlib.sha256(payload).digest()
    return header + payload


header = bytearray(64)
struct.pack_into('<4sII IHHII', header, 0, b'GPA1', 1, 0, 1, 8, 8, len(texture(0x5a)), 0x3f800000)
sequences = struct.pack('<4H', 0, 1, 0, 0) * 30
frame = struct.pack('<4H2h', 0, 0, 8, 8, -4, -4)
garment = seal(header, sequences + frame + texture(0x5a))
(root / 'romfs:/clothing/429.gpa').write_bytes(garment)
(root / 'romfs:/clothing/430.gpa').write_bytes(garment)
bad = bytearray(garment)
bad[-1] ^= 1
(root / 'romfs:/clothing/431.gpa').write_bytes(bad)

header = bytearray(64)
struct.pack_into('<4s6I2H', header, 0, b'GPA2', 2, 0, 2, 2, 2, 0x3f800000, 24, 0)
poses = struct.pack('<6H', 0, 1, 0, 0, 1, 1) + struct.pack('<6H', 1, 1, 0, 1, 2, 1)
offset = 64 + len(poses) + len(frame) * 2 + 32
pages = struct.pack('<IIHHI', offset, len(texture(0x33)), 8, 8, 0)
pages += struct.pack('<IIHHI', offset + len(texture(0x33)), len(texture(0xa5)), 8, 8, 0)
(root / 'assets/sprite.gpa').write_bytes(seal(header, poses + frame * 2 + pages + texture(0x33) + texture(0xa5)))

header = bytearray(64)
struct.pack_into('<4sIII', header, 0, b'GPK1', 1, 0, 2)
(root / 'assets/movie.gpk').write_bytes(seal(header, struct.pack('<4I', 80, 1, 81, 1) + b'ab'))
(root / 'assets/texture.t3x').write_bytes(texture(0x5a))

header = bytearray(64)
large = texture(0x77, 512)
struct.pack_into('<4sII IHHII', header, 0, b'GPA1', 1, 0, 1, 512, 512, len(large), 0x3f800000)
path = root / 'romfs:/clothing/1001.gpa'
path.write_bytes(seal(header, sequences + frame + large))
for identity in range(1002, 1025):
    os.link(path, root / f'romfs:/clothing/{identity}.gpa')
