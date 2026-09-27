"""Run in isolated ALS research UE project. Read/export only; never save assets.

Sample every source AnimSequence at up to 31 evenly spaced times, plus exact
float-curve keys. These samples do not certify all in-between frames or gameplay.
"""
import unreal as u
import json
import gzip
import math
from pathlib import Path

out = Path(u.Paths.project_dir()).resolve().parent
(out / 'als-graphs').mkdir(exist_ok=True)
registry = u.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(['/ALS'], force_rescan=True)
options = u.AnimPoseEvaluationOptions()
options.should_retarget = False
options.retrieve_additive_as_full_pose = True
bones = ['root', 'pelvis', 'spine_01', 'spine_02', 'spine_03', 'neck_01', 'head',
         'clavicle_l', 'upperarm_l', 'lowerarm_l', 'hand_l',
         'clavicle_r', 'upperarm_r', 'lowerarm_r', 'hand_r',
         'thigh_l', 'calf_l', 'foot_l', 'thigh_r', 'calf_r', 'foot_r',
         'ik_hand_root', 'ik_hand_gun', 'ik_hand_l', 'ik_hand_r']
clips = []
graphs_only = 'ReferenceGraphsOnly' in u.SystemLibrary.get_command_line()
assets = [] if graphs_only else registry.get_assets_by_path('/ALS/ALS/Animations', recursive=True)
for data in assets:
    if str(data.asset_class_path.asset_name) != 'AnimSequence':
        continue
    asset = data.get_asset()
    assert asset, str(data.package_name)
    # Drain source/base-pose compression before evaluating additive clips.
    # UE 5.8 evaluation overlapping their compilation otherwise deadlocked
    # in this research commandlet. This helper exists only in the study project.
    u.MotionResearchLibrary.finish_reference_compilation()
    length = asset.sequence_length
    count = min(30, max(1, math.ceil(length * 30)))
    frames = []
    for i in range(count + 1):
        t = length * i / count
        pose = u.AnimPoseExtensions.get_anim_pose_at_time(asset, t, options)
        available = set(map(str, u.AnimPoseExtensions.get_bone_names(pose)))
        points = {}
        for bone in bones:
            if bone not in available:
                continue
            tr = u.AnimPoseExtensions.get_bone_pose(pose, bone, u.AnimPoseSpaces.WORLD)
            points[bone] = {'p': [tr.translation.x, tr.translation.y, tr.translation.z],
                            'q': [tr.rotation.x, tr.rotation.y, tr.rotation.z, tr.rotation.w]}
        frames.append({'time': t, 'bones': points})
    names = u.AnimationLibrary.get_animation_curve_names(asset, u.RawCurveTrackTypes.RCT_FLOAT)
    curves = {}
    for name in names:
        times, values = u.AnimationLibrary.get_float_keys(asset, name)
        curves[str(name)] = {'times': list(times), 'values': list(values)}
    clips.append({'path': asset.get_path_name(), 'name': asset.get_name(), 'duration': length,
                  'additive_type': str(asset.get_editor_property('additive_anim_type')),
                  'curves': curves, 'samples': frames})
    (out / 'als-samples.partial.json.gz').write_bytes(gzip.compress(json.dumps(clips).encode(), mtime=0))
    u.log('MATURE_REFERENCE_CLIP ' + asset.get_name())
if not graphs_only:
    (out / 'als-samples.json.gz').write_bytes(gzip.compress(json.dumps(clips).encode(), mtime=0))
else:
    clips = json.loads(gzip.decompress((out / 'als-samples.json.gz').read_bytes()))
graph_count = 0
for data in registry.get_assets_by_path('/ALS/ALS/Character', recursive=True):
    if str(data.asset_class_path.asset_name) != 'AnimBlueprint':
        continue
    asset = data.get_asset()
    assert asset, str(data.package_name)
    task = u.AssetExportTask()
    task.object = asset
    task.filename = str(out / 'als-graphs' / (asset.get_name() + '.t3d'))
    task.automated = True
    task.prompt = False
    task.exporter = u.ObjectExporterT3D()
    assert u.Exporter.run_asset_export_task(task), str(data.package_name)
    graph_count += 1
rig = u.load_asset('/ALS/ALS/Character/CR_Als')
assert rig
task = u.AssetExportTask()
task.object = rig
task.filename = str(out / 'als-graphs/CR_Als.t3d')
task.automated = True
task.prompt = False
task.exporter = u.ObjectExporterT3D()
assert u.Exporter.run_asset_export_task(task)
u.log('MATURE_REFERENCE_COMPLETE clips=' + str(len(clips)) + ' samples=' + str(sum(len(c['samples']) for c in clips)) + ' graphs=' + str(graph_count))
