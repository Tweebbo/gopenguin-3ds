"""Compare native Old/New profiles with and without a loopback background update."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import json
import shutil
import struct
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
output = root / 'build/background-emulator'
output.mkdir(exist_ok=True)
resource = (root / 'romfs/games/fire.gpk').read_bytes()
index = b'GPRIDX1\0' + struct.pack('<II', 1, len(resource))
index += b'games/fire.gpk'.ljust(128, b'\0') + hashlib.sha256(resource).digest() + struct.pack('<I', len(resource))
bundle = b'GPRBND1\0' + struct.pack('<I', 1) + hashlib.sha256(resource).digest() + struct.pack('<I', len(resource)) + resource
payload = b'3DSX' + b'\x5a' * (24 * 1024 * 1024 - 4)
cia = bytearray(payload)
cia[:32] = bytes(32)
struct.pack_into('<I', cia, 0, 32)
struct.pack_into('>I', cia, 64, 0x10004)
struct.pack_into('>Q', cia, 64 + 0x140 + 0x4c, 0x0004000007E90200)
struct.pack_into('>H', cia, 64 + 0x140 + 0x9c, (1 << 10) | (3 << 4) | 10)
azahar = Path('/home/miguelv/Tweebbo-3DS/build/emu/squashfs-root')
cases = [(family, background, saver) for family, saver in [('old', False), ('new', False), ('new', True)] for background in [False, True]]
ui_only = '--ui' in sys.argv
if ui_only:
    cases = [('old', True, False), ('new', True, False)]
app = root / 'out/gopenguin-background-final-test.3dsx'
if '--family' in sys.argv:
    selected = sys.argv[sys.argv.index('--family') + 1]
    cases = [case for case in cases if case[0] == selected]

def run(case):
    family, background, saver = case
    name = family + ('-saver' if saver else '') + ('-background' if background else '-control')
    if ui_only:
        name += '-ui'
    folder = output / name
    binary_sha = hashlib.sha256(app.read_bytes()).hexdigest()
    if (folder / 'result.txt').exists() and (folder / 'provenance.json').exists():
        if json.loads((folder / 'provenance.json').read_text())['binarySha256'] == binary_sha:
            return name
        (folder / 'result.txt').unlink()
    folder.mkdir(exist_ok=True)
    config = folder / 'home/.config/azahar-emu'
    config.mkdir(parents=True, exist_ok=True)
    original = root / f'build/performance/full-{family}3ds/home'
    shutil.copy(original / '.config/azahar-emu/qt-config.ini', config / 'qt-config.ini')
    old_sd = original / '.local/share/azahar-emu/sdmc/3ds/gopenguin'
    sd = folder / 'home/.local/share/azahar-emu/sdmc/3ds/gopenguin'
    sd.mkdir(parents=True, exist_ok=True)
    if (sd / 'resources').exists():
        shutil.rmtree(sd / 'resources')
    for pattern in ['performance-case', 'performance-done', 'performance.csv', 'performance-cases.csv', 'last-update.log', 'background-ui-case', 'background-ui-assertions.csv', 'background-ui-ready-race', '*.part', '*.sha256']:
        for previous in sd.glob(pattern):
            previous.unlink()
    for path in old_sd.glob('performance-snapshot-*.json'):
        shutil.copy(path, sd / path.name)
    if saver:
        (sd / 'battery-saver-test').write_text('1\n')
    for flag in (['performance-enabled', 'load-test'] if ui_only else ['performance-auto', 'performance-enabled', 'load-test']):
        (sd / flag).write_text('1\n')
    if background:
        (sd / 'background-update-auto').write_text('1\n')
    if ui_only:
        (sd / 'background-ui-auto').write_text('1\n')
    fixture = folder / 'fixture'
    fixture.mkdir(exist_ok=True)
    shutil.copy(root / 'tools/background-fixture-server.py', fixture / 'server.py')
    shutil.copy(root / 'tools/background-emu-session.sh', folder / 'emu-session.sh')
    app_payload = payload[:2 * 1024 * 1024] if ui_only else payload
    app_cia = cia[:2 * 1024 * 1024] if ui_only else cia
    for filename, content in [('app.3dsx', app_payload), ('app.cia', app_cia), ('resources.idx', index), ('resources.gpb', bundle)]:
        (fixture / filename).write_bytes(content)
    def artifact(filename, content):
        return dict(url='http://127.0.0.1:18891/' + filename, bytes=len(content), sha256=hashlib.sha256(content).hexdigest())
    manifest = dict(schema=1, titleId='0004000007E90200', version='1.3.10', cia=artifact('app.cia', app_cia), **{'3dsx': artifact('app.3dsx', app_payload)})
    manifest['resources'] = dict(schema=1, index=artifact('resources.idx', index), bundle=artifact('resources.gpb', bundle), deltas=[])
    (fixture / 'latest.json').write_text(json.dumps(manifest))
    server = 'gp-background-server-' + name
    for container in [server, 'gp-background-emu-' + name]:
        previous = subprocess.run(['podman', 'inspect', '--format', '{{.State.Running}}', container], capture_output=True, text=True)
        if previous.returncode == 0:
            if previous.stdout.strip() == 'true':
                raise RuntimeError('A review is already running in ' + container)
            subprocess.run(['podman', 'rm', container], check=True, capture_output=True)
    subprocess.run(['podman', 'run', '-d', '--rm', '--name', server, '--network', 'none', '-v', str(fixture) + ':/fixtures:z', 'localhost/native-3ds-libctru-fixed:latest', 'python3', '/fixtures/server.py'], check=True, capture_output=True)
    command = ['podman', 'run', '--rm', '--name', 'gp-background-emu-' + name, '--network', 'container:' + server, '--cpus', '4', '--memory', '2g', '-e', 'BACKGROUND_APP=/work/' + str(app.relative_to(root)), '-v', str(root) + ':/work:z', '-v', str(folder) + ':/emu:z', '-v', str(azahar) + ':/azahar:ro,z', 'localhost/tweebbo-3ds-emu-rec:latest', 'bash', '/emu/emu-session.sh']
    try:
        with (folder / 'session.log').open('w') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=600)
    finally:
        subprocess.run(['podman', 'stop', '-t', '1', server], capture_output=True)
    provenance = dict(emulator='Azahar', profile=family, background=background, batterySaver=saver, physicalConsole=False, cpuClockPercentage=100, resolutionFactor=1, renderer='software OpenGL', containerCPUs=4, network='isolated container loopback only', syntheticUpdateBytes=len(app_payload), syntheticApplicationInstalled=False, binarySha256=binary_sha)
    (folder / 'provenance.json').write_text(json.dumps(provenance, indent=2) + '\n')
    print('PASS', name, flush=True)
    return name

with ThreadPoolExecutor(max_workers=2) as executor:
    list(executor.map(run, cases))
