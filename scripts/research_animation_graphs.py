"""Read UE ObjectExporterT3D output, retaining pose-pin edges and source lines.

Static graph evidence only: not a simulation of a live Animation Blueprint.
Third-party exports stay in Saved; summaries can be stored with research notes.
"""
from pathlib import Path
import argparse
import json
import re
import hashlib


def parse(path):
    raw = path.read_bytes()
    text = raw.decode('utf-16') if raw.startswith((b'\xff\xfe', b'\xfe\xff')) else raw.decode('utf8')
    objects, stack = {}, []
    for line_number, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if line.startswith('Begin Object'):
            name = re.search(r'Name="([^"]+)"', line)
            if not name:
                stack.append(None)
                continue
            export = re.search(r'ExportPath="([^"]+)"', line)
            key = export[1] if export else '/'.join([x or '?' for x in stack] + [name[1]])
            obj = objects.setdefault(key, {'name': name[1], 'path': key, 'properties': {}, 'pins': []})
            obj['line'] = line_number
            cls = re.search(r'Class=([^ ]+)', line)
            if cls:
                obj['class'] = cls[1]
            stack.append(key)
        elif line == 'End Object':
            stack.pop()
        elif stack and stack[-1]:
            obj = objects[stack[-1]]
            if line.startswith('CustomProperties Pin'):
                name = re.search(r'PinName="([^"]+)"', line)
                linked = re.search(r'LinkedTo=\(([^)]*)\)', line)
                default = re.search(r'(?<!\w)DefaultValue="([^"]*)"', line)
                obj['pins'].append({'name': name[1], 'output': 'EGPD_Output' in line,
                                    'pose': '.PoseLink' in line or '.ComponentSpacePoseLink' in line,
                                    'default': default[1] if default else None,
                                    'linked_nodes': re.findall(r'(\w+) [A-F0-9]+', linked[1]) if linked else [],
                                    'line': line_number})
            elif '=' in line and not line.startswith(('ShowPinForProperties', 'CustomProperties')):
                k, v = line.split('=', 1)
                if k in ('Node', 'NodeComment', 'CacheName', 'LayerGroup'):
                    obj['properties'][k] = v
    nodes = [o for o in objects.values() if 'AnimGraphNode_' in o.get('class', '')]
    return {'input': str(path), 'sha256': hashlib.sha256(raw).hexdigest(), 'nodes': nodes}


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('input', type=Path)
    p.add_argument('output', type=Path)
    a = p.parse_args()
    result = [parse(f) for f in sorted(a.input.glob('*.t3d'))]
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(result, indent=2, ensure_ascii=False))
    for item in result:
        print(Path(item['input']).name, len(item['nodes']), 'animation nodes')
