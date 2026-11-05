"""Compare staged resources through the actual sanitized C runtime readers."""
import json
from pathlib import Path
import subprocess
import sys

report = json.loads(Path(sys.argv[1]).read_text())
source, output = Path(sys.argv[2]), Path(sys.argv[3])
counts = {'movie': 0, 'audio': 0}
for row in report['files']:
    if row['sourceSha256'] == row['sha256']:
        continue
    kind = 'movie' if row['file'].endswith('.gpk') else 'audio'
    subprocess.run(['build/lossless-resources-test', kind, str(source / row['file']), str(output / row['file'])], check=True)
    counts[kind] += 1
print('PASS ASan/UBSan runtime readers:', counts, 'byte-identical decoded textures, metadata, scenes and PCM; rewind verified.')
