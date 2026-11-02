#!/usr/bin/env python3
"""Compare exported SWL art with original vectors at the native display size."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zlib
from PIL import Image, ImageChops, ImageDraw, ImageStat

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline-cache', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
work = root / 'build/sharpness'
baseline = work / 'baseline-romfs'
digest = lambda data: hashlib.sha256(data).hexdigest()


def parts(file):
    data = file.read_bytes()
    assert data[:4] in (b'GPK1', b'GPK2')
    assert digest(data[64:]) == data[32:64].hex()
    packed = data[:4] == b'GPK2'
    result = []
    for i in range(struct.unpack_from('<I', data, 12)[0]):
        fields = struct.unpack_from('<IIII' if packed else '<II', data, 64 + i * (16 if packed else 8))
        offset, size = fields[:2]
        part = data[offset:offset + size]
        if packed and fields[3]:
            assert fields[3] == 1
            part = zlib.decompress(part)
        result.append(part)
    return result


results = []
cases = [(entry['room'], False, 1, 'root') for entry in json.loads((work / 'rooms.json').read_text())]
cases += [('pizzatron', True, 2, 'root'), ('cart', True, 1, 'root'), ('beans', True, 2, 'board'), ('fish', True, 2, 'root')]
for name, game, frame, clip in cases:
    key = 'game-' + name if game else name
    old = args.baseline_cache / key
    new = root / 'build/scenes' / key
    before = parts(baseline / 'games' / (name + '.gpk'))[0] if game else (baseline / 'scenes' / (name + '.swl')).read_bytes()
    assert digest(before) == json.loads((old / 'manifest.json').read_text())['binary'], name + ': cached baseline differs'
    if game:
        for i, texture in enumerate(parts(baseline / 'games' / (name + '.gpk'))[2:]):
            assert texture == (old / f'atlas-{i}.t3x').read_bytes(), name + ': cached texture differs'
    before_scene = work / name / 'before.swl'
    before_scene.write_bytes(before)
    for label, zoom in [('overview', 1), ('zoom', 1.2)]:
        source = Image.open(work / name / ('source-' + label + '.png')).convert('RGBA')
        source = Image.alpha_composite(Image.new('RGBA', source.size, 'white'), source).convert('RGB')
        images = [source]
        metrics = {}
        for tag, scene, atlases in [('before', before_scene, old), ('after', new / 'scene.swl', new)]:
            out = work / name / (tag + '-' + label)
            out.mkdir(exist_ok=True)
            env = dict(os.environ, SWL_RENDER_SCALE=str(400 / 760 * zoom), SWL_RENDER_ROOT=clip, SWL_RENDER_FRAME=str(frame))
            subprocess.run([str(work / 'render'), str(scene), str(atlases), str(out), '0'], env=env, check=True, stdout=subprocess.DEVNULL)
            image = Image.frombytes('RGBA', (400, 240), (out / 'native-0.rgba').read_bytes())
            image = Image.alpha_composite(Image.new('RGBA', image.size, 'white'), image).convert('RGB')
            image.save(out / 'native.png')
            images.append(image)
            metrics[tag] = sum(ImageStat.Stat(ImageChops.difference(source, image)).mean) / 3
        sheet = Image.new('RGB', (1200, 264), 'white')
        draw = ImageDraw.Draw(sheet)
        for i, (caption, image) in enumerate(zip(['Original vectors', 'Before', 'After'], images)):
            draw.text((i * 400 + 8, 5), f'{name} / {label} / {caption}', fill='black')
            sheet.paste(image, (i * 400, 24))
        sheet.save(work / name / (label + '-comparison.png'))
        result = dict(name=name, view=label, frame=frame, meanAbsoluteRgbError=metrics)
        results.append(result)
        print(f'{name:10} {label:8} RGB MAE {metrics["before"]:.3f} -> {metrics["after"]:.3f}')
(work / 'comparison.json').write_text(json.dumps(results, indent=2) + '\n')
fixture = work / 'gpu-romfs'
for tag, source in [('before', baseline), ('after', root / 'assets/sharpness/romfs')]:
    for kind, name, suffix in [('scenes', 'town', '.swl'), ('gfx', 'swl-town-0', '.t3x')]:
        destination = fixture / kind / (tag + suffix)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source / kind / (name + suffix), destination)
