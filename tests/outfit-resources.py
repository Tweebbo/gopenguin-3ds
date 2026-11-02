#!/usr/bin/env python3
"""Check packaged original outfit resources through the native parsers."""
import hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parent.parent
manifest=json.loads((root/'assets/outfits/manifest.json').read_text())
checks=[]
for resource in manifest['results']:
    file=root/'romfs'/resource['kind']/f"{resource['id']}.gpa"
    assert file.stat().st_size==resource['bytes']
    assert hashlib.sha256(file.read_bytes()).hexdigest()==resource['sha256']
    subprocess.run([str(root/'build/outfit-resources-test'),str(file),*map(str,resource['actions'])],check=True,capture_output=True)
    checks.append({'kind':resource['kind'],'id':resource['id'],'actions':len(resource['actions']),'bytes':resource['bytes'],'dynamic':resource.get('dynamic',False)})
result={'status':'PASS','resources':len(checks),'avatarActions':len(manifest['avatarActions']),'bytes':sum(c['bytes'] for c in checks),'missingOriginalClothing':manifest['missingClothing'],'checks':checks}
(root/'build/outfit-resources.json').write_text(json.dumps(result,indent=2)+'\n')
print(f"PASS {len(checks)} outfit banks: native hash/metadata/texture/pose/cycle validation; {len(manifest['avatarActions'])} original avatar actions")
