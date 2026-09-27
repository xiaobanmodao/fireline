"""Measure chest-relative segment changes in existing traces, without art gates."""
from pathlib import Path
from collections import defaultdict
import argparse
import csv
import math
import json
import statistics


def sub(a, b): return tuple(x-y for x, y in zip(a, b))
def dot(a, b): return sum(x*y for x, y in zip(a, b))
def cross(a, b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def unrotate(q, v):
    xyz = tuple(-x for x in q[:3]); t = tuple(2*x for x in cross(xyz, v))
    return tuple(v[i]+q[3]*t[i]+cross(xyz, t)[i] for i in range(3))
def angle(a, b): return math.degrees(math.acos(max(-1, min(1, dot(a, b)/math.sqrt(dot(a, a)*dot(b, b))))))


def measure(folder):
    data = defaultdict(dict)
    for r in csv.DictReader((folder/'poses.csv').open()):
        g, t, s = int(r['group']), float(r['time']), int(r['stage'])
        if g < 20 and .75 <= t <= 1.4 and s in (2, 3, 5, 6, 7, 8):
            data[g, t, s][r['bone']] = ([float(r[k]) for k in ('x','y','z')], [float(r[k]) for k in ('qx','qy','qz','qw')])
    groups = defaultdict(lambda: defaultdict(list))
    for g, t in sorted({(g,t) for g,t,s in data}):
        for a, b in [(2,5), (5,3), (6,7), (7,8), (8,3)]:
            if (g,t,a) not in data or (g,t,b) not in data: continue
            pa, pb = data[g,t,a], data[g,t,b]
            for side in ('L','R'):
                for name, x, y in [('upper','UpperArm','LowerArm'),('forearm','LowerArm','DJ_wrist')]:
                    va=unrotate(pa['Chest'][1],sub(pa[y+'_'+side][0],pa[x+'_'+side][0]))
                    vb=unrotate(pb['Chest'][1],sub(pb[y+'_'+side][0],pb[x+'_'+side][0]))
                    groups[f'group_{g}_stage_{a}_to_{b}'][name+'_'+side].append(angle(va,vb))
    return {g:{k:{'n':len(v),'median_deg':statistics.median(v),'max_deg':max(v)} for k,v in values.items()} for g,values in groups.items()}


if __name__ == '__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
    result={'input':str(a.folder),'window_seconds':[.75,1.4],
            'stages':{'2':'adapted weapon/action layer','5':'BodyPose','6':'adapted Contact input, NOT donor','7':'Contact spine registration','8':'Contact targets','3':'final'},
            'note':'Changes remove common chest rotation. Not collision/anatomy tests; a large difference alone is not a defect.',
            'groups':measure(a.folder)}
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2))
