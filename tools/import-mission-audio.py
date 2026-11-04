#!/usr/bin/env python3
"""Export every original PSA mission sound and map scene cues, ambience and voice.

Mission 3 uses the explicit PC cue IDs. Missions 4-11 use the first soundFiles
entry as the scene cue. The longest sound >= 5 s in a scene becomes its ambient
loop; the shortest 0.4-3 s sound different from the cue becomes its voice cue.
"""
import json, os, subprocess, tempfile, shutil
from pathlib import Path

root = Path(__file__).resolve().parents[1]
web = Path('/home/miguelv/gopenguin-dev/web')
pack_audio = Path('/home/miguelv/Tweebbo-Penguin-3DS/tools/pack-audio.mjs')
mission3 = {'vault': 90, 'office': 86, 'hq': 380, 'roof': 72}

def duration(path):
    result = subprocess.run(['ffprobe', '-v', 'error', '-show_entries', 'format=duration', '-of', 'default=nw=1:nk=1', str(path)], capture_output=True, text=True, check=True)
    return float(result.stdout.strip())

missions = {}
for mission in range(3, 12):
    if mission == 3:
        scenes = [(scene, mission3.get(scene, 0), f'q3-{scene}') for scene in ('vault','office','hq','roof','boiler','fusebox','nightclub')]
    else:
        data = json.loads((web / f'public/data/mission{mission}.json').read_text())
        scenes = []
        for scene, info in data['scenes'].items():
            pack = info['pack'].strip('/').replace('swf/', '', 1)
            timeline = json.loads((web / f'public/swf/{pack}/timelines.json').read_text())
            ids = [int(value) for value in timeline.get('soundFiles', {})]
            scenes.append((scene, ids[0] if ids else 0, pack, ids))
    missions[mission] = scenes

named = json.loads((root / 'romfs/audio/named-sounds.json').read_text())
manifest = json.loads((root / 'romfs/audio/manifest.json').read_text())
manifest['effects'] = [e for e in manifest['effects'] if not e['bank'].startswith('mission')]
for mission, scenes in missions.items():
    bank = f'mission{mission}'
    if mission == 3:
        sources = {}
        for scene, cue, pack in scenes:
            timeline = json.loads((web / f'public/swf/{pack}/timelines.json').read_text())
            for sound in timeline.get('soundFiles', {}):
                sources[int(sound)] = web / 'public/swf' / pack / 'chars' / f'sound_{sound}.mp3'
    else:
        sources = {}
        for scene, cue, pack, ids in scenes:
            timeline = json.loads((web / f'public/swf/{pack}/timelines.json').read_text())
            for sound in timeline.get('soundFiles', {}):
                sources[int(sound)] = web / 'public/swf' / pack / 'chars' / f'sound_{sound}.mp3'
    work = Path(tempfile.mkdtemp(prefix=f'{bank}-'))
    source = work / 'src' / 'sfx' / bank
    source.mkdir(parents=True)
    names, durations, effect_of = {}, {}, {}
    effect = 1
    for sound, path in sorted(sources.items()):
        if not path.exists():
            print(f'{bank}: missing {path}')
            continue
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(path), '-ac', '1', '-ar', '22050', '-c:a', 'pcm_s16le', str(source / f'{effect}.wav')], check=True)
        names[f'snd{sound}'] = effect
        durations[sound] = duration(source / f'{effect}.wav')
        effect_of[sound] = effect
        effect += 1
    if not effect_of:
        shutil.rmtree(work)
        continue
    output = work / 'out'
    subprocess.run(['node', str(pack_audio), f'--source={work / "src"}', f'--output={output}'], check=True)
    target = root / 'romfs/audio/sfx' / bank
    target.mkdir(parents=True, exist_ok=True)
    for gpz in (output / 'sfx' / bank).glob('*.gpz'):
        shutil.copy2(gpz, target / gpz.name)
    scene_first = []
    for index, entry in enumerate(scenes):
        scene, cue = entry[0], entry[1]
        ids = entry[3] if mission != 3 else [int(v) for v in json.loads((web / f'public/swf/q3-{scene}/timelines.json').read_text()).get('soundFiles', {})]
        if cue and cue in effect_of:
            names[f'scene{index}'] = effect_of[cue]
            scene_first.append(cue)
        ambient = max((sound for sound in ids if durations.get(sound, 0) >= 5), key=lambda sound: durations[sound], default=0)
        if ambient:
            names[f'amb{index}'] = effect_of[ambient]
        voice = min((sound for sound in ids if sound != cue and 0.4 <= durations.get(sound, 0) <= 3), key=lambda sound: durations[sound], default=0)
        if voice:
            names[f'voice{index}'] = effect_of[voice]
    named[bank] = names
    manifest['soundBanks'][bank] = bank
    compression = json.loads((output / 'compression.json').read_text())
    by_id = {int(Path(entry['logical']).stem): entry['sourceBytes'] for entry in compression['files']}
    for effect_id, size in by_id.items():
        manifest['effects'].append({'bank': bank, 'id': effect_id, 'bytes': size, 'streamed': size > 131072})
    print(f'{bank}: {len(effect_of)} sounds, {sum(1 for k in names if k.startswith("amb"))} ambience, {sum(1 for k in names if k.startswith("voice"))} voices')
    shutil.rmtree(work)
(root / 'romfs/audio/named-sounds.json').write_text(json.dumps(named, indent=2) + '\n')
(root / 'romfs/audio/manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('Updated named-sounds.json and manifest.json')
