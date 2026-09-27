"""UE Python: read installed Epic full-body clips; never modify/save assets."""
import unreal as u
import json,math,hashlib
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve();out=root/'Saved/NativeRifleReference';out.mkdir(exist_ok=True)
paths=['/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload','/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Equip','/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Idle_ADS','/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Start','/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Apex','/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Fall_Land']
options=u.AnimPoseEvaluationOptions();options.should_retarget=False
clips=[]
for path in paths:
 asset=u.load_asset(path);assert asset,path
 frames=[];length=asset.sequence_length;count=max(1,math.ceil(length*60))
 for i in range(count+1):
  pose=u.AnimPoseExtensions.get_anim_pose_at_time(asset,length*i/count,options)
  points={}
  for b in ('pelvis','spine_01','spine_02','spine_03','neck_01','head','clavicle_l','upperarm_l','lowerarm_l','hand_l','clavicle_r','upperarm_r','lowerarm_r','hand_r'):
   t=u.AnimPoseExtensions.get_bone_pose(pose,b,u.AnimPoseSpaces.WORLD);points[b]=[t.translation.x,t.translation.y,t.translation.z]
  frames.append({'t':length*i/count,'points':points})
 file=root/'Content'/path.removeprefix('/Game/');file=file.with_suffix('.uasset')
 clips.append({'path':path,'duration':length,'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'frames':frames})
(out/'samples.json').write_text(json.dumps(clips))
u.log('NATIVE_RIFLE_REFERENCE clips='+str(len(clips))+' frames='+str(sum(len(c['frames']) for c in clips)))
