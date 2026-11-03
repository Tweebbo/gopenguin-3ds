"""Serve synthetic updates over container loopback; never install the fixture app."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import re

root = Path('/fixtures')

class Handler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def do_GET(self):
        name = self.path.split('?', 1)[0]
        allowed = {'/latest.json', '/app.3dsx', '/app.cia', '/resources.idx', '/resources.gpb'}
        if name not in allowed:
            self.send_error(404)
            return
        path = root / name[1:]
        total = path.stat().st_size
        first, last = 0, total - 1
        requested = self.headers.get('Range', '')
        match = re.fullmatch(r'bytes=(\d+)-(\d*)', requested)
        if requested and not match:
            self.send_error(416)
            return
        if match:
            first = int(match[1])
            if match[2]:
                last = int(match[2])
        if not 0 <= first <= last < total:
            self.send_error(416)
            return
        self.send_response(206 if match else 200)
        self.send_header('Content-Length', str(last - first + 1))
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('ETag', '"isolated-fixture-v1"')
        if match:
            self.send_header('Content-Range', f'bytes {first}-{last}/{total}')
        self.end_headers()
        try:
            with path.open('rb') as f:
                f.seek(first)
                left = last - first + 1
                while left:
                    data = f.read(min(left, 65536))
                    self.wfile.write(data)
                    left -= len(data)
        except (BrokenPipeError, ConnectionResetError):
            pass
        with (root / 'requests.jsonl').open('a') as log:
            log.write(json.dumps(dict(path=name, first=first, last=last)) + '\n')

    def log_message(self, *_args):
        pass

ThreadingHTTPServer(('127.0.0.1', 18891), Handler).serve_forever()
