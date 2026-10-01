"""Read original mesh post-process and raw clip at captured native times.

Runs in the isolated ReferenceProject. Never saves source assets. Native replay
is a single-node clip plus the mesh post-process, not the Lyra gameplay graph.
"""
import csv, gzip, json
from pathlib import Path
import unreal as u

project = Path(u.Paths.project_dir()).resolve()
out = project.parent / 'ADSLayoutStudy'
run = project / 'Saved/LyraADSReview/lyra-native-ads-clean-20261001'
mesh = u.load_asset('/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny')
clip = u.load_asset('/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Idle_ADS')
assert mesh and clip and (run / 'poses.csv').exists()
cls = mesh.get_editor_property('post_process_anim_blueprint')
assert cls
path = cls.get_path_name()
bp = u.load_asset(path.removesuffix('_C'))
assert bp
task = u.AssetExportTask()
task.object = bp
task.filename = str(out / 'lyra-postprocess.t3d')
task.automated = True
task.prompt = False
task.exporter = u.ObjectExporterT3D()
assert u.Exporter.run_asset_export_task(task)
with (out / 'lyra.bind.csv').open() as f:
    names = [r['bone'] for r in csv.DictReader(f)]
with (run / 'poses.csv').open() as f:
    times = {int(r['frame']): float(r['time']) for r in csv.DictReader(f)}
options = u.AnimPoseEvaluationOptions()
options.should_retarget = False
options.retrieve_additive_as_full_pose = True
frames = []
for frame, time in sorted(times.items()):
    pose = u.AnimPoseExtensions.get_anim_pose_at_time(clip, time % clip.sequence_length, options)
    bones = {}
    for name in names:
        t = u.AnimPoseExtensions.get_bone_pose(pose, name, u.AnimPoseSpaces.WORLD)
        bones[name] = {'p': list(t.translation.to_tuple()),
                       'q': [t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w],
                       's': list(t.scale3d.to_tuple())}
    frames.append({'frame': frame, 'time': time, 'bones': bones})
(out / 'lyra-raw-at-native-times.json.gz').write_bytes(gzip.compress(json.dumps(frames).encode(), mtime=0))
(out / 'lyra-postprocess.json').write_text(json.dumps({
    'mesh': mesh.get_path_name(), 'post_process_class': path,
    'captured_run': run.name, 'raw_frames': len(frames), 'bone_count': len(names),
    'assets_saved': False, 'final_gameplay_graph': False,
}, indent=2))
u.log('LYRA_ADS_POSTPROCESS_COMPLETE frames=' + str(len(frames)) + ' class=' + path)
