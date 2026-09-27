"""Run read-only in the installed official Lyra project; export contact evidence."""
import unreal as u
import json
import gzip
from pathlib import Path

out = Path(__file__).resolve().parents[1] / 'unreal/Fireline/Saved/MatureMotionResearch/lyra-contacts'
out.mkdir(parents=True, exist_ok=True)
base = '/Game/Characters/Heroes/Mannequin/Animations'
paths = [base + '/Actions/' + n for n in ['MM_Rifle_Reload', 'MM_Rifle_Reload_Additive', 'MM_Rifle_Equip', 'MM_Rifle_Equip_Additive']]
paths += [base + '/Locomotion/Rifle/' + n for n in ['MM_Rifle_Idle_ADS', 'MM_Rifle_Jog_Fwd', 'MM_Rifle_Walk_Fwd']]
opt = u.AnimPoseEvaluationOptions()
opt.should_retarget = False
opt.retrieve_additive_as_full_pose = True
records = []
for path in paths:
    asset = u.load_asset(path)
    assert asset, path
    if hasattr(u, 'MotionResearchLibrary'):
        u.MotionResearchLibrary.finish_reference_compilation()
    curves = {}
    for name in u.AnimationLibrary.get_animation_curve_names(asset, u.RawCurveTrackTypes.RCT_FLOAT):
        t, v = u.AnimationLibrary.get_float_keys(asset, name)
        curves[str(name)] = {'times': list(t), 'values': list(v)}
    frames = []
    for i in range(61):
        t = asset.sequence_length * i / 60
        pose = u.AnimPoseExtensions.get_anim_pose_at_time(asset, t, opt)
        points = {}
        for bone in map(str, u.AnimPoseExtensions.get_bone_names(pose)):
            if not any(k in bone for k in ['hand', 'weapon', 'arm', 'clavicle', 'spine', 'neck', 'pelvis', 'head']):
                continue
            tr = u.AnimPoseExtensions.get_bone_pose(pose, bone, u.AnimPoseSpaces.WORLD)
            points[bone] = {'p': [tr.translation.x, tr.translation.y, tr.translation.z],
                            'q': [tr.rotation.x, tr.rotation.y, tr.rotation.z, tr.rotation.w]}
        frames.append({'time': t, 'bones': points})
    records.append({'path': path, 'name': asset.get_name(), 'duration': asset.sequence_length,
                    'additive_type': str(asset.get_editor_property('additive_anim_type')), 'curves': curves, 'samples': frames})
    (out / 'samples.json.gz').write_bytes(gzip.compress(json.dumps(records).encode(), mtime=0))
for path in [base + '/Locomotion/Rifle/ABP_RifleAnimLayers', base + '/LinkedLayers/ABP_ItemAnimLayersBase',
             '/Game/Weapons/Rifle/Animations/AM_MM_Rifle_Reload', '/Game/Weapons/Rifle/Animations/AM_MM_Rifle_Equip']:
    if 'ReferenceSourceOnly' in u.SystemLibrary.get_command_line() and '/ABP_' in path:
        continue  # Isolated content study lacks Lyra's native AnimInstance class.
    asset = u.load_asset(path)
    assert asset, path
    objects = [asset]
    if isinstance(asset, u.AnimBlueprint):
        objects.append(u.get_default_object(asset.generated_class()))
    for obj in objects:
        task = u.AssetExportTask()
        task.object = obj
        task.filename = str(out / (obj.get_name().replace('Default__', 'CDO_') + '.t3d'))
        task.automated = True
        task.prompt = False
        task.exporter = u.ObjectExporterT3D()
        assert u.Exporter.run_asset_export_task(task), path
u.log('LYRA_CONTACT_COMPLETE clips=' + str(len(records)))
