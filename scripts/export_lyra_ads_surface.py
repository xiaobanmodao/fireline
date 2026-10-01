"""Export original ADS clip and actual render surfaces in the isolated host."""
import unreal as u
from pathlib import Path
import csv,json,gzip

out=Path(u.Paths.project_dir()).resolve().parent/'ADSLayoutStudy'
out.mkdir(exist_ok=True)
for name,path in [('lyra','/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny'),('lyra-rifle','/Game/Weapons/Rifle/Mesh/SK_Rifle')]:
    mesh=u.load_asset(path);assert mesh,path
    u.MotionResearchLibrary.finish_reference_compilation()
    assert u.MotionResearchLibrary.export_mesh_geometry(mesh,str(out/name))
clip=u.load_asset('/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Idle_ADS');assert clip
options=u.AnimPoseEvaluationOptions();options.should_retarget=False;options.retrieve_additive_as_full_pose=True
with (out/'lyra.bind.csv').open() as f:names=[r['bone'] for r in csv.DictReader(f)]
frames=[]
for i in range(61):
    time=clip.sequence_length*i/60;pose=u.AnimPoseExtensions.get_anim_pose_at_time(clip,time,options)
    available=set(map(str,u.AnimPoseExtensions.get_bone_names(pose)));assert set(names)<=available,set(names)-available
    bones={}
    for n in names:
        t=u.AnimPoseExtensions.get_bone_pose(pose,n,u.AnimPoseSpaces.WORLD)
        bones[n]={'p':[t.translation.x,t.translation.y,t.translation.z],'q':[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],'s':[t.scale3d.x,t.scale3d.y,t.scale3d.z]}
    frames.append({'time':time,'bones':bones})
(out/'lyra-ads-full.json.gz').write_bytes(gzip.compress(json.dumps({'asset':clip.get_path_name(),'frames':frames,'bones':names,'native_clip_only':True,'source_game_graph':False}).encode(),mtime=0))
u.log('LYRA_ADS_SURFACE_COMPLETE frames=61 bones='+str(len(names))+' source_game_graph=0 assets_saved=0')
