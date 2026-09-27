"""Check isolated review geometry/data; this does not grant visual acceptance."""
from pathlib import Path
import csv, gzip, json, argparse
import numpy as np
from scipy.spatial.transform import Rotation as R

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / 'unreal/Fireline/Saved/MatureMotionResearch/ReferenceProject/Saved'
OUT = DATA / 'WholeCarryReview'

def rows(path):
    with path.open(encoding='utf-8-sig') as stream:
        return list(csv.DictReader(stream))

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--proportions',action='store_true');args=parser.parse_args()
    global OUT
    if args.proportions:OUT=DATA/'BodyProportionReview'
    target = json.loads((DATA / ('BodyProportionStudy/target.json' if args.proportions else 'WholeCarrySource/target.json')).read_text())
    bind, hold = target['reference'], target['hold']
    poses = json.loads(gzip.decompress((OUT / 'poses.json.gz').read_bytes()))['contact']
    vertices = np.array([[float(row[k]) for k in ['x', 'y', 'z']]
                         for row in rows(OUT / 'native-bind-vertices.csv')])
    groups = {}
    for row in rows(OUT / 'native-skin-weights.csv'):
        groups.setdefault(row['bone'], []).append((int(row['vertex']), int(row['weight']) / 65535))
    totals = np.zeros(len(vertices))
    dominant = np.empty(len(vertices), dtype=object)
    maximum = np.zeros(len(vertices))
    skin = {}
    for bone, items in groups.items():
        ids = np.array([i for i, _ in items])
        weights = np.array([w for _, w in items])
        totals[ids] += weights
        greater = weights > maximum[ids]
        dominant[ids[greater]] = bone
        maximum[ids[greater]] = weights[greater]
        b = bind[bone]
        local = R.from_quat(b['q']).inv().apply(vertices[ids] - b['p']) / b['s']
        skin[bone] = ids, weights, local
    sole_heights = [];intrusions=[]
    from scipy.spatial import ConvexHull
    torso=np.isin(dominant,["Chest","Torso"])
    distal=np.array([isinstance(n,str) and (n.startswith("DJ_") or n.startswith("LowerArm_")) for n in dominant])
    for pose in poses:
        skinned = np.zeros_like(vertices)
        for bone, (ids, weights, local) in skin.items():
            p = pose[bone]
            skinned[ids] += (R.from_quat(p['q']).apply(local * p['s']) + p['p']) * weights[:, None]
        if args.proportions:
            hull=ConvexHull(skinned[torso]).equations
            distances=(np.einsum('ij,kj->ik',skinned[distal],hull[:,:3],optimize=False)+hull[:,3]).max(axis=1)
            intrusions.append({'frame':len(intrusions),'deepest_cm':float(max(0,-distances.min())),'inside_vertex_count':int((distances<-.01).sum())})
        # Actual native viewer scene registration from measured boot sole bounds.
        sole_heights.append([float(skinned[dominant == 'Foot_' + s, 2].min() + .70) for s in ['L', 'R']])
    soles = np.array(sole_heights)
    parity = []
    for path in sorted(OUT.glob('pose-*.csv')):
        captured = rows(path)
        if 'time' not in captured[0]:
            continue
        index = round(float(captured[0]['time']) * 60)
        err = max(np.linalg.norm(np.array([float(row[k]) for k in ['x', 'y', 'z']])
                                 - poses[index][row['bone']]['p']) for row in captured)
        parity.append(float(err))
    lengths = {}
    step = {}
    grip_error = []
    def position(pose, name):
        return np.array(pose[name]['p'])
    for side in ['L', 'R']:
        for parent, child in [('UpperArm', 'LowerArm'), ('LowerArm', 'DJ_wrist'),
                              ('UpperLeg', 'LowerLeg'), ('LowerLeg', 'Foot')]:
            a, b = parent + '_' + side, child + '_' + side
            expected = np.linalg.norm(position(bind, a) - position(bind, b))
            lengths[a + '->' + b] = max(abs(np.linalg.norm(position(p, a) - position(p, b)) - expected) for p in poses)
        for name in ['UpperArm_', 'LowerArm_', 'DJ_wrist_']:
            n = name + side
            trajectory = np.array([position(p, n) for p in poses])
            step[n] = float(np.linalg.norm(np.diff(trajectory, axis=0), axis=1).max())
        wrist = 'DJ_wrist_' + side
        hgun = hold['M4_body']
        expected = R.from_quat(hgun['q']).inv().apply(position(hold, wrist) - hgun['p'])
        for p in poses:
            gun = p['M4_body']
            relative = R.from_quat(gun['q']).inv().apply(position(p, wrist) - gun['p'])
            grip_error.append(float(np.linalg.norm(relative - expected)))
    report = {
        'frames': len(poses), 'fps': 60, 'skinned_vertices': len(vertices),
        'weight_sum_error_max': float(abs(totals - 1).max()),
        'native_capture_samples': len(parity),
        'native_component_position_error_cm_max': max(parity) if parity else None,
        'limb_length_error_cm_max_by_segment': lengths,
        'receiver_relative_wrist_error_cm_max': max(grip_error),
        'joint_step_cm_max_at_60fps': step,
        'sole_world_height_cm': {s: {'min': float(soles[:, i].min()), 'max': float(soles[:, i].max())}
                                 for i, s in enumerate(['L', 'R'])},
        'lowest_sole_height_cm_max': float(soles.min(axis=1).max()),
        'chest_convex_envelope_screen':{'max_depth_cm':max((r['deepest_cm'] for r in intrusions),default=0),'frames_with_inside_vertices':sum(r['inside_vertex_count']>0 for r in intrusions)},
        'visual_acceptance': False,
        'limits': ['M4 recorded idle-walk-stop only; not live game input',
                   'No triangle-intersection guarantee or all-state art acceptance',
                   'Constant grip registration differs substantially from raw source carry'],
    }
    (OUT / 'audit.json').write_text(json.dumps(report, indent=2))
    if args.proportions:(OUT/'chest-envelope-screen.json').write_text(json.dumps(intrusions))
    print(json.dumps(report, indent=2))
    assert len(parity) == (56 if args.proportions else 28), 'Complete final native capture before delivery'
    assert max(parity) < .001
    assert max(lengths.values()) < .001
    assert max(grip_error) < .001
    assert abs(totals - 1).max() < .0001
    assert soles.min() >= -.01
    assert soles.min(axis=1).max() < 1

if __name__ == '__main__':
    main()
