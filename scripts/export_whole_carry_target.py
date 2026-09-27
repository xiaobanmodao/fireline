"""Read native Ryan/DJ/M4 reference and selected hold; save no UE asset."""
import unreal as u
import json
from pathlib import Path

out=Path(u.Paths.project_saved_dir()).resolve()/'WholeCarrySource'
out.mkdir(parents=True,exist_ok=True)
mesh=u.load_asset('/Game/Fireline/Hands/SK_RyanAction')
hold=u.load_asset('/Game/Fireline/SycgffM4/A_M4_Idle')
assert mesh and hold
u.MotionResearchLibrary.finish_reference_compilation()
comp=u.SkeletalMeshComponent();comp.set_skeletal_mesh(mesh)
opt=u.AnimPoseEvaluationOptions();opt.should_retarget=False
ref=u.AnimPoseExtensions.get_reference_pose(mesh.skeleton)
pose=u.AnimPoseExtensions.get_anim_pose_at_time(hold,0,opt)
def tr(t):
    return {'p':list(t.translation.to_tuple()),'q':[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],
            's':list(t.scale3d.to_tuple())}
names=list(map(str,u.AnimPoseExtensions.get_bone_names(pose)))
parents={n:str(comp.get_parent_bone(n)) for n in names}
result={'mesh':mesh.get_path_name(),'hold_asset':hold.get_path_name(),'names':names,
        'parents':{n:p if p in names else None for n,p in parents.items()},
        'reference':{n:tr(u.AnimPoseExtensions.get_bone_pose(ref,n,u.AnimPoseSpaces.WORLD)) for n in names},
        'hold':{n:tr(u.AnimPoseExtensions.get_bone_pose(pose,n,u.AnimPoseSpaces.WORLD)) for n in names}}
(out/'target.json').write_text(json.dumps(result))
u.log('WHOLE_CARRY_TARGET_EXPORTED bones='+str(len(names)))
