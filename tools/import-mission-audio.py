#!/usr/bin/env python3
"""Export the original PSA scene-change sounds for the 3DS mission banks.

Mission 3 uses the same explicit IDs as the PC view. Missions 4-11 use the
first soundFiles entry of the scene pack, matching mission-flow.ts.
"""
import json, subprocess, tempfile, shutil
from pathlib import Path

root = Path(__file__).resolve().parents[1]
web = Path('/home/miguelv/gopenguin-dev/web')
pack_audio = Path('/home/miguelv/Tweebbo-Penguin-3DS/tools/pack-audio.mjs')

mission3 = {'vault': 90, 'office': 86, 'hq': 380, 'roof': 72}
missions = {}
for mission in range(3, 12):
    if mission == 3:
        scenes = [(scene, mission3.get(scene, 0), f'q3-{scene}') for scene in ('vault', 'office', 'hq', 'roof', 'boiler', 'fusebox', 'nightclub')]
    else:
        data = json.loads((web / f'public/data/mission{mission}.json').read_text())
        scenes = []
        for scene, info in data['scenes'].items():
            pack = info['pack'].strip('/').replace('sfx/', '', 1)
            if pack.startswith('swf/'):
                pack = pack[4:]
            timeline = json.loads((web / f'public/swf/{pack}/timelines.json').read_text())
            ids = list(timeline.get('soundFiles', {}))
            scenes.append((scene, int(ids[0]) if ids else 0, pack))
    missions[mission] = scenes

named = json.loads((root / 'romfs/audio/named-sounds.json').read_text())
manifest = json.loads((root / 'romfs/audio/manifest.json').read_text())
manifest['effects'] = [e for e in manifest['effects'] if not e['bank'].startswith('mission')]
for mission, scenes in missions.items():
    bank = f'mission{mission}'
    work = Path(tempfile.mkdtemp(prefix=f'{bank}-'))
    source = work / 'src' / 'sfx' / bank
    source.mkdir(parents=True)
    names = {}
    effect = 1
    for index, (scene, sound, pack) in enumerate(scenes):
        if not sound:
            continue
        mp3 = web / 'public/swf' / pack / 'chars' / f'sound_{sound}.mp3'
        if not mp3.exists():
            print(f'{bank} scene {scene}: missing {mp3}')
            continue
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(mp3), '-ac', '1', '-ar', '22050', '-c:a', 'pcm_s16le', str(source / f'{effect}.wav')], check=True)
        names[f'scene{index}'] = effect
        effect += 1
    if not names:
        shutil.rmtree(work)
        continue
    output = work / 'out'
    subprocess.run(['node', str(pack_audio), f'--source={work / "src"}', f'--output={output}'], check=True)
    target = root / 'romfs/audio/sfx' / bank
    target.mkdir(parents=True, exist_ok=True)
    for gpz in (output / 'sfx' / bank).glob('*.gpz'):
        shutil.copy2(gpz, target / gpz.name)
    named[bank] = names
    manifest['soundBanks'][bank] = bank
    compression = json.loads((output / 'compression.json').read_text())
    by_id = {int(Path(entry['logical']).stem): entry['sourceBytes'] for entry in compression['files']}
    for sound_name, effect_id in names.items():
        manifest['effects'].append({'bank': bank, 'id': effect_id, 'bytes': by_id[effect_id], 'streamed': by_id[effect_id] > 131072})
    print(f'{bank}: {len(names)} scene sounds')
    shutil.rmtree(work)

(root / 'romfs/audio/named-sounds.json').write_text(json.dumps(named, indent=2) + '\n')
manifest['music'] = sorted(set(manifest['music']))
(root / 'romfs/audio/manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('Updated named-sounds.json and manifest.json')
