"""Audit original-source equipment parity and raw vs post-process skin.

Saved diagnostics only; no Fireline asset writes, retarget, or eye acceptance.
"""
import argparse, csv, gzip, json
from collections import Counter
import numpy as np
from body_proportion_geometry import Surface, rows
from build_whole_carry_candidate import compose, transform
from study_surface_mount import BASE, REF, SurfaceMount, load_pose
from mesh_contact_geometry import triangle_crossings


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--run', default='lyra-native-ads-clean-20261001')
    parser.add_argument('--raw-run', default='lyra-native-ads-raw-20261001')
    args = parser.parse_args()
    source = BASE / 'ADSLayoutStudy'
    run = BASE / 'ReferenceProject/Saved/LyraADSReview' / args.run
    raw_run = run.parent / args.raw_run
    raw_capture = json.loads((raw_run / 'capture.json').read_text())
    assert not raw_capture.get('initial_source_mesh_post_process', raw_capture.get('source_mesh_post_process', True))
    raw = json.loads(gzip.decompress((source / 'lyra-raw-at-native-times.json.gz').read_bytes()))
    meta = json.loads((source / 'lyra-postprocess.json').read_text())
    assert meta['captured_run'] == args.run
    equipment = json.loads((source / 'lyra-equipment.json').read_text())['actors'][0]
    component = compose(transform(equipment['components'][0]['world_at_identity_actor']),
                        transform(equipment['equipment_relative']))
    gun_bind = {r['bone']: transform({'p': [float(r[k]) for k in ['x', 'y', 'z']],
                                    'q': [float(r[k]) for k in ['qx', 'qy', 'qz', 'qw']],
                                    's': [float(r[k]) for k in ['sx', 'sy', 'sz']]})
                for r in rows(source / 'lyra-rifle.bind.csv')}
    body = Surface(source / 'lyra')
    gun = Surface(source / 'lyra-rifle')
    position_error = rotation_error = scale_error = 0.
    differences = {}
    surfaces = []
    skin_changes = []
    for sample in raw:
        frame = sample['frame']
        actual = load_pose(run / 'poses.csv', frame)
        # Isolate post-process using two actual native replay paths. The UE
        # extraction API defaults to Raw and is independently compared by
        # audit_lyra_ads_toggle.py; it is not the runtime-compressed baseline.
        raw_pose = load_pose(raw_run / 'poses.csv', frame)
        actual_gun = load_pose(run / 'gun-poses.csv', frame)
        mounting = compose(component, actual[equipment['socket']])
        for name, bind in gun_bind.items():
            expected = compose(bind, mounting)
            position_error = max(position_error, float(np.linalg.norm(expected[0] - actual_gun[name][0])))
            rotation_error = max(rotation_error, float(np.degrees((expected[1] * actual_gun[name][1].inv()).magnitude())))
            scale_error = max(scale_error, float(np.max(np.abs(expected[2] - actual_gun[name][2]))))
        for name in actual:
            p = float(np.linalg.norm(actual[name][0] - raw_pose[name][0]))
            q = float(np.degrees((actual[name][1] * raw_pose[name][1].inv()).magnitude()))
            prev = differences.setdefault(name, {'position_cm': 0., 'rotation_deg': 0.})
            prev['position_cm'] = max(prev['position_cm'], p)
            prev['rotation_deg'] = max(prev['rotation_deg'], q)
        if frame in [0, 44, 74, 104]:
            bv = body.deform(actual)
            gv = gun.deform(actual_gun)
            displacement = np.linalg.norm(bv - body.deform(raw_pose), axis=1)
            shoulder = np.array([n.startswith(('clavicle_', 'upperarm_')) for n in body.dom])
            skin_changes.append({'frame': frame, 'all_vertices_max_cm': float(displacement.max()),
                                 'shoulder_vertices': int(shoulder.sum()),
                                 'shoulder_max_cm': float(displacement[shoulder].max()),
                                 'shoulder_over_1cm': int((displacement[shoulder] > 1).sum())})
            # Complete native neck/head and spine/clavicle face groups. Fingers
            # intentionally excluded from this scope; they touch the gun.
            face_labels = body.dom[body.tris]
            head = np.array([any(n.startswith(('head', 'neck_')) for n in label) for label in face_labels])
            torso = np.array([any(n.startswith(('spine_', 'clavicle_')) for n in label) for label in face_labels])
            surfaces.append({'frame': frame, 'head_neck_faces': int(head.sum()),
                             'spine_clavicle_faces': int(torso.sum()),
                             'head_neck_gun_pairs': len(triangle_crossings(bv[body.tris[head]], gv[gun.tris])),
                             'spine_clavicle_gun_pairs': len(triangle_crossings(bv[body.tris[torso]], gv[gun.tris]))})
    assert position_error < 1e-3 and rotation_error < 1e-3 and scale_error < 1e-5
    ranked = sorted(differences.items(), key=lambda item: item[1]['position_cm'], reverse=True)
    # Localize the earlier FAILED transfer without hiding any broad chest faces.
    s = SurfaceMount(BASE / 'LiveCarryProject/Saved/LiveAim/restored-profile-moving-20260930-30', 90)
    s.body = Surface(REF.parent / 'ChestPanelStudy/candidate')
    failed = BASE / 'LyraADSChainStudy/source-contact-probe-pose.json'
    pose = {n: transform(v) for n, v in json.loads(failed.read_text()).items()}
    pairs = triangle_crossings(s.body.deform(pose)[s.body.tris[s.chest]], s.gun.deform(pose)[s.gun.tris])
    groups = Counter('/'.join(sorted(set(s.body.dom[s.body.tris[s.chest[a]]]))) for a, b in pairs)
    report = {'source_post_process': meta, 'native_comparison_without_post_process': args.raw_run,
              'captured_frames': len(raw), 'native_mesh_bones': len(differences),
              'equipment_parity_max_position_cm': position_error, 'equipment_parity_max_rotation_deg': rotation_error,
              'equipment_parity_max_scale': scale_error, 'top_raw_native_bone_differences': ranked[:12],
              'source_raw_native_skin': skin_changes, 'source_surface_samples': surfaces,
              'failed_transfer_chest_gun_pairs': len(pairs), 'failed_transfer_pair_regions': dict(groups),
              'raw_clip_is_final_native_surface': False, 'final_lyra_gameplay_graph': False,
              'anatomical_eye_validated': False, 'fireline_pose_imported': False, 'visual_acceptance': False}
    (source / 'native-source-audit.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
