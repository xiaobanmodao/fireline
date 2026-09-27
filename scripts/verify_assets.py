"""Verify the separately stored local asset snapshot. No downloads or payments."""
from pathlib import Path
import hashlib,json,sys
p=Path(__file__).resolve().parents[1]/'unreal/Fireline'
items=json.loads((p/'Docs/local-assets.json').read_text())['files'];bad=[]
for item in items:
 f=p/item['path']
 if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=item['sha256']:bad.append(item['path'])
print(f'{len(items)} assets checked; {len(bad)} missing or changed')
for name in bad[:20]:print(name)
sys.exit(bool(bad))
