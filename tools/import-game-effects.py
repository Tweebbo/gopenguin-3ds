#!/usr/bin/env python3
"""Export original Rescue and Paint By Letters effects for the native audio player."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
web = Path(os.environ.get('GOPENGUIN_WEB', Path.home() / 'gopenguin-dev/web'))
audio = root / 'romfs/audio'
names = json.loads((audio / 'named-sounds.json').read_text())
manifest = json.loads((audio / 'manifest.json').read_text())
banks = ('rescue', 'book1', 'book2', 'book3')
manifest['effects'] = [item for item in manifest['effects'] if item['bank'] not in banks]
records = []
for bank in banks:
    pack = web / 'public/swf' / bank
    timeline = json.loads((pack / 'timelines.json').read_text())
    aliases = {name: int(sound) for sound, name in timeline.get('exports', {}).items()
               if sound in timeline.get('soundFiles', {})}
    if bank == 'rescue':
        aliases = {'puffle': 404, 'rescue': 406, 'wipeout': 99}
    names[bank] = aliases
    target = audio / 'sfx' / bank
    target.mkdir(parents=True, exist_ok=True)
    for sound in sorted(set(aliases.values())):
        source = pack / 'chars' / timeline['soundFiles'][str(sound)]
        output = target / f'{sound}.wav'
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(source),
                        '-ac', '1', '-ar', '22050', '-c:a', 'pcm_s16le', str(output)], check=True)
        size = output.stat().st_size
        manifest['effects'].append({'bank': bank, 'id': sound, 'bytes': size, 'streamed': size > 128 * 1024})
        relative = output.relative_to(audio).as_posix()
        record = {'path': relative, 'bytes': size, 'sha256': hashlib.sha256(output.read_bytes()).hexdigest()}
        manifest.setdefault('files', {})[relative] = record
        records.append({**record, 'sourceSha256': hashlib.sha256(source.read_bytes()).hexdigest()})
    manifest.setdefault('soundBanks', {})[bank] = bank
    print(f'{bank}: {len(set(aliases.values()))} original effects')
(audio / 'named-sounds.json').write_text(json.dumps(names, indent=2) + '\n')
(audio / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
(audio / 'game-effects-sources.json').write_text(json.dumps(records, indent=2) + '\n')
