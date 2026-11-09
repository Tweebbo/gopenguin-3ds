#!/usr/bin/env python3
"""Compare native outfit selection with the shared PC/server rule data."""
import json
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parent.parent
web=Path(os.environ.get('GOPENGUIN_WEB',root/'../../GoPenguin-Gameplay-PC-20261101/web')).resolve()
rules=json.loads((web/'src/engine/frame-hacks.json').read_text())
slots=['head','face','neck','body','hand','feet']
items=set(map(int,(root/'source/outfit_assets.h').read_text().split('{',1)[1].split('}',1)[0].split(',')))
extra_header=(root/'source/outfit_extra_assets.h').read_text()
extra_items=set(map(int,extra_header.split('outfit_extra_items[]={',1)[1].split('}',1)[0].split(',')))
extra_actions=set(map(int,extra_header.split('outfit_extra_actions[]={',1)[1].split('}',1)[0].split(',')))
cases=[]
for action,combinations in rules.items():
    selected={tuple(h[s] for s in slots):h['secret_frame'] for h in combinations}
    for look,secret in selected.items():
        if 30<secret<=76:
            available=items
        elif 76<secret and secret in extra_actions:
            available=extra_items
        else:
            available=set()
        expected=secret if available and all(not i or i in available for i in look) else int(action)
        cases.append((int(action),look,secret,expected))
cases.extend([(26,(424,0,0,240,0,0),33,33),(26,(424,0,0,240,0,1),26,26),(26,(0,0,0,0,0,0),26,26),(26,(429,0,0,0,0,0),36,36)])
text=''.join(' '.join(map(str,[a,*look]))+'\n' for a,look,_,_ in cases)
result=subprocess.run([str(root/'build/outfit-rules-test')],input=text,text=True,capture_output=True,check=True)
actual=[list(map(int,line.split())) for line in result.stdout.splitlines()]
assert len(actual)==len(cases)
for (action,look,secret,expected),values in zip(cases,actual):
    assert values==[secret,expected],(action,look,values,[secret,expected])
print(f'PASS {len(cases)} native outfit cases: shared rule precedence, available original art, pizza/helmet selection and incompatible extra clothing')
