#!/usr/bin/env python3
"""Stage existing banks and recorded mission states for offline GPU screenshots."""
import argparse
import json
from pathlib import Path
import shutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--trace', type=Path, required=True)
parser.add_argument('--guide-pages', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
trace = json.loads(args.trace.read_text())
cases = []
for index, title in [(1, 'Gary dialogue'), (6, 'Secret word entry'), (20, 'Mountain signs'),
                     (30, 'Prototype sled trial'), (71, 'Inventory after the crash'),
                     (73, 'Survival guide: first page'), (140, 'Cave fire and water'), (157, 'Mission complete')]:
    snapshot = trace[index]['snapshot']
    if snapshot.get('mission') != 2:
        raise ValueError('Expected the reviewed mission 2 trace')
    cases.append(dict(game='mission2', sourceIndex=index, title=title, snapshot=snapshot))
for game in ['cart', 'pizzatron']:
    for playing in [False, True]:
        title = ('Cart Surfer' if game == 'cart' else 'Pizzatron 3000') + (' gameplay' if playing else ' title')
        cases.append(dict(game=game, playing=playing, title=title))
if args.guide_pages:
    pages = json.loads((root / 'build/agent-tests/guide-readability.json').read_text())
    for page in pages:
        if not page['allOriginalTextMatched']:
            raise ValueError('Run the guide text checks before capturing it')
        source_index = 71 + page['page']
        snapshot = trace[source_index]['snapshot']
        if snapshot.get('phase') != 'guide' or snapshot.get('guidePage') != page['page']:
            raise ValueError('Guide snapshot mapping differs')
        for part in range(page['parts']):
            cases.append(dict(game='mission2', group='guide', sourceIndex=source_index,
                              guidePage=page['page'], guidePart=part, guideParts=page['parts'],
                              title=f"Survival guide: page {page['page']}, text {part + 1} of {page['parts']}", snapshot=snapshot))
output = root / 'build/native-review'
romfs = output / 'romfs'
for folder in ['missions/2', 'gfx', 'games']:
    (romfs / folder).mkdir(parents=True, exist_ok=True)
(romfs / 'cases.json').write_text(json.dumps(cases) + '\n')
(output / 'cases.json').write_text(json.dumps([{k: v for k, v in case.items() if k != 'snapshot'} for case in cases], indent=2) + '\n')
(output / 'case-count.txt').write_text(str(len(cases)) + '\n')
for source in (root / 'romfs/missions/2').glob('*.gpk'):
    shutil.copy2(source, romfs / 'missions/2' / source.name)
for name in ['cart', 'cart-title', 'pizzatron', 'pizzatron-title']:
    shutil.copy2(root / 'romfs/games' / (name + '.gpk'), romfs / 'games' / (name + '.gpk'))
shutil.copy2(root / 'romfs/gfx/font.t3x', romfs / 'gfx/font.t3x')
print(f'Staged {len(cases)} native capture cases and existing packaged banks, without modifying production resources.')
