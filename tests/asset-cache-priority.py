"""Exercise the real cache worker, HTTP transport and package validator on loopback."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import os
import subprocess
import tempfile
import threading
import time

root = Path(__file__).resolve().parent.parent
build = root / 'build/igloo-cache-tests'
build.mkdir(parents=True, exist_ok=True)
flags = ['-std=c11', '-D_POSIX_C_SOURCE=200809L', '-DNO_TLS', '-O2', '-Wall', '-Wextra', '-Werror', '-Isource']
if os.environ.get('SANITIZE'):
    flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
subprocess.run(['cc', *flags, 'tests/asset-cache-priority.c', 'source/asset_cache.c', 'source/package.c',
                'source/sprite.c', 'source/garment.c', 'source/hash.c', 'source/common.c',
                'source/transport/conn.c', 'source/transport/ws.c', 'source/transport/download.c',
                '-lcrypto', '-lz', '-pthread', '-o', str(build / 'worker')], cwd=root, check=True)
payload = (root / 'romfs/data/basic-igloo.gpk').read_bytes()
requests = []
mode = ''
marker = None

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        requests.append(self.path)
        if mode == 'priority' and self.path == '/books/1.gpk' and requests.count(self.path) == 1:
            marker.touch()
            # An unrelated server that never sends headers must not block entry.
            time.sleep(3)
        data = b'invalid' if self.path == '/movies/200003.gpk' else payload
        self.send_response(404 if self.path == '/movies/200004.gpk' else 200)
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass

server = ThreadingHTTPServer(('127.0.0.1', 18896), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
results = []
try:
    for mode in ['fifo', 'priority']:
        requests.clear()
        with tempfile.TemporaryDirectory(dir=build) as temporary:
            folder = Path(temporary)
            marker = folder / 'started'
            result = subprocess.run([str(build / 'worker'), str(folder / 'cache'), str(marker), mode],
                                    capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, result.stdout + result.stderr
            books = [p for p in requests if p.startswith('/books/')]
            expected = [f'/books/{i}.gpk' for i in range(1, 33)]
            assert books == (['/books/1.gpk'] + expected if mode == 'priority' else expected), books
            if mode == 'priority':
                assert requests[:3] == ['/books/1.gpk', '/movies/200002.gpk', '/books/1.gpk'], requests[:3]
            assert requests.count('/movies/200002.gpk') == 1
            assert not list((folder / 'cache').glob('*.tmp'))
            assert all(path.read_bytes() == payload for path in (folder / 'cache').glob('*.gpa'))
            results.append(dict(mode=mode, status='PASS', output=result.stdout.strip(), requests=requests.copy()))
finally:
    server.shutdown()
(build / 'report.json').write_text(json.dumps(dict(status='PASS', loopbackOnly=True, results=results), indent=2) + '\n')
print('\n'.join(r['output'] for r in results))
