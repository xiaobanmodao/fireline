"""Read the selected sycgff native M4 operation tracks; no asset writes."""
import unreal as u
import json
from pathlib import Path
out=Path(u.Paths.project_saved_dir()).resolve()/'M4OperationSource';out.mkdir(parents=True,exist_ok=True)
u.MotionResearchLibrary.finish_reference_compilation()
opt=u.AnimPoseEvaluationOptions();opt.should_retarget=False
result={'fps':60,'provider':'sycgff / selected Fireline native adaptation','clips':{}}
for name in ['Idle','Draw','Remove','Reload','TacticalReload']:
 asset=u.load_asset('/Game/Fireline/SycgffM4/A_M4_'+name);assert asset,name
 duration=asset.sequence_length
 frames=[]
 for f in range(round(duration*60)+1):
  pose=u.AnimPoseExtensions.get_anim_pose_at_time(asset,min(f/60,duration),opt)
  names=list(map(str,u.AnimPoseExtensions.get_bone_names(pose)))
  frame={}
  for n in names:
   t=u.AnimPoseExtensions.get_bone_pose(pose,n,u.AnimPoseSpaces.WORLD)
   frame[n]={'p':list(t.translation.to_tuple()),'q':[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],'s':list(t.scale3d.to_tuple())}
  frames.append(frame)
 result['clips'][name]={'asset':asset.get_path_name(),'duration':duration,'frames':frames}
(out/'actions.json').write_text(json.dumps(result))
u.log('M4_OPERATION_SOURCE_EXPORTED '+str({n:len(c['frames']) for n,c in result['clips'].items()}))
