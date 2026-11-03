"""Bind the navigation report to native entry traces, fault events and binary hashes."""
from pathlib import Path
import hashlib
import json
import re

root = Path(__file__).resolve().parent.parent
folder = root / 'build/igloo-reliability'
review = root.parent / 'review/igloo-reliability'
cases = ['normal', 'reuse', 'drop', 'delay', 'custom', 'custom-reuse', 'mission-return', 'recovery', 'catalogue-return']
profiles = {}
for family in ['old', 'new']:
    run = folder / family
    assert (run / 'result.txt').read_text().startswith('PASS:')
    text = (run / 'protocol.log').read_text()
    entries = [dict(zip(['owner','type','reused','loadMilliseconds','totalMilliseconds'], map(int, match)))
               for match in re.findall(r'igloo entered owner=(\d+) type=(\d+) reused=(\d+) load_ms=(\d+) total_ms=(\d+)', text)]
    assert len(entries) == len(cases), entries
    assert entries[1]['reused'] == entries[5]['reused'] == 1
    assert 'igloo stopped action=3' in text and 'catalogue closed id=' in text
    provenance = json.loads((run / 'provenance.json').read_text())
    assert provenance['binarySha256'] == hashlib.sha256((run / 'app.3dsx').read_bytes()).hexdigest()
    events = json.loads((run / 'network.json').read_text())
    phases = []
    for event in events:
        if event['event'] == 'mode':
            phases.append(dict(mode=event['mode'], time=event['time'], requests=0))
        elif event['event'] == 'gm-request':
            phases[-1]['requests'] += 1
    assert [p['requests'] for p in phases if p['mode'] == 'drop'] == [2]
    assert [p['requests'] for p in phases if p['mode'] == 'delay'] == [2]
    assert [p['requests'] for p in phases if p['mode'] == 'timeout'] == [3]
    assets = [e for e in events if e['event'] == 'asset']
    blocked = next(e for e in assets if e['path'].endswith('/books/900001.gpk'))
    dependency = next(e for e in assets if e['path'].endswith('/movies/200002.gpk'))
    assert 0 < dependency['time'] - blocked['time'] < 25000
    profiles[family] = dict(provenance=provenance, cases=dict(zip(cases, entries)),
                            requestPhases=phases, assetPreemptionMilliseconds=dependency['time'] - blocked['time'])
cache = json.loads((root / 'build/igloo-cache-tests/report.json').read_text())
report = dict(status='PASS', physicalConsoleTested=False, deployed=False, profiles=profiles, cache=cache,
              scope='Native Old/New Azahar profiles, real isolated localhost server, fault-injection proxy. Synthetic style 2 uses the unchanged embedded basic bank. No physical Wi-Fi/SD or battery measurement.')
review.mkdir(exist_ok=True)
(review / 'measurements.json').write_text(json.dumps(report, indent=2) + '\n')
print('PASS: 18 native entries, repeated-tap request counts, asset preemption, timeout/recovery and catalogue closure.')
