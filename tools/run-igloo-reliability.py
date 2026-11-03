"""Run native Old/New navigation cases against an isolated real server and fault proxy."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import time
import sys

root = Path(__file__).resolve().parent.parent
output = root / 'build/igloo-reliability'
azahar = Path('/home/miguelv/Tweebbo-3DS/build/emu/squashfs-root')
app = root / 'out/gopenguin-igloo-reliability-test.3dsx'
started = int(time.time() * 1000)

def run(family):
    folder = output / family
    folder.mkdir(exist_ok=True)
    for previous in [*folder.glob('*.png'), folder / 'result.txt', folder / 'protocol.log']:
        previous.unlink(missing_ok=True)
    config = folder / 'home/.config/azahar-emu'
    config.mkdir(parents=True, exist_ok=True)
    shutil.copy(root / f'build/performance/full-{family}3ds/home/.config/azahar-emu/qt-config.ini', config / 'qt-config.ini')
    sd = folder / 'home/.local/share/azahar-emu/sdmc/3ds/gopenguin'
    if sd.exists():
        shutil.rmtree(sd)
    sd.mkdir(parents=True)
    shutil.copy(output / f'{family}-settings.json', sd / 'settings.json')
    shutil.copy(app, folder / 'app.3dsx')
    shutil.copy(root / 'tools/igloo-reliability-emu.sh', folder / 'emu-session.sh')
    command = ['podman', 'run', '--rm', '--name', f'gp-igloo-reliability-{family}', '--network',
               'container:gp-igloo-reliability-server', '--cpus', '4', '--memory', '2g', '-e', f'IGLOO_FAMILY={family}',
               '-v', str(folder) + ':/emu:z', '-v', str(azahar) + ':/azahar:ro,z',
               'localhost/tweebbo-3ds-emu-rec:latest', 'bash', '/emu/emu-session.sh']
    with (folder / 'session.log').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=360)
    provenance = dict(emulator='Azahar', profile=family, physicalConsole=False, cpuClockPercentage=100,
                      resolutionFactor=1, renderer='software OpenGL', containerCPUs=4,
                      network='isolated container loopback only', customStyle='synthetic type 2 using the unchanged basic bank',
                      binarySha256=hashlib.sha256((folder / 'app.3dsx').read_bytes()).hexdigest())
    (folder / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    events = [json.loads(line) for line in (output / 'network.jsonl').read_text().splitlines()]
    (folder / 'network.json').write_text(json.dumps([e for e in events if e['family'] == family and e['time'] >= started], indent=2) + '\n')
    print('PASS', family, flush=True)

with ThreadPoolExecutor(max_workers=2) as executor:
    families = [sys.argv[sys.argv.index('--family') + 1]] if '--family' in sys.argv else ['old', 'new']
    assert all(family in ['old', 'new'] for family in families)
    list(executor.map(run, families))
