"""Presentation measurements on actual native pose traces, not art acceptance."""
import argparse,csv,json
from pathlib import Path
import numpy as np
from scipy.spatial.transform import Rotation as R
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('directory',type=Path);p.add_argument('--require-refined',action='store_true');a=p.parse_args();d=a.directory
st=list(csv.DictReader((d/'states.csv').open(encoding='utf-8-sig')));n=len(st)
names=['Chest','UpperArm_R','M4_body','M4_frontsight','M4_rearsight','DJ_wrist_L','DJ_wrist_R','LowerArm_L','LowerArm_R','Foot_L','Foot_R']
pos={b:np.zeros((n,3)) for b in names};q={b:np.zeros((n,4)) for b in names}
for row in csv.DictReader((d/'poses.csv').open(encoding='utf-8-sig')):
 b=row['bone'];i=int(row['frame'])
 if b in pos:pos[b][i]=[float(row[k]) for k in ['x','y','z']];q[b][i]=[float(row[k]) for k in ['qx','qy','qz','qw']]
v=lambda k:np.array([float(s[k]) for s in st]);dt=v('dt')[1:];aim=v('aiming_weight')>.999
ref=ROOT/'unreal/Fireline/Saved/MatureMotionResearch/ReferenceProject/Saved/BodyProportionStudy/target.json';t=json.loads(ref.read_text());reg=json.loads((ROOT/'unreal/Fireline/Docs/Validation/move-aim-refinement/registration.json').read_text())
stock=pos['M4_body']+(R.from_quat(q['M4_body'])*R.from_quat(t['hold']['M4_body']['q']).inv()).apply(np.array(reg['stock_hold_cm'])-t['hold']['M4_body']['p'])
ch=R.from_quat(q['Chest'])*R.from_quat(t['reference']['Chest']['q']).inv();delta=ch.inv().apply(stock-pos['UpperArm_R'])-reg['stock_from_shoulder_chest_cm']
locked=[];rot=R.from_euler('z',v('actor_yaw')-90,degrees=True);location=np.stack([v(k) for k in ['actor_x','actor_y','actor_z']],axis=1)
for side,key in [('L','left_lock'),('R','right_lock')]:
 world=rot.apply(pos['Foot_'+side])+location;velocity=np.linalg.norm(np.diff(world[:,:2],axis=0),axis=1)/dt;mask=(v(key)[1:]>.999)&(v(key)[:-1]>.999);locked.extend(velocity[mask].tolist())
# Chest-relative speed separates pose transition from capsule translation.
speeds={}
for b in ['DJ_wrist_L','DJ_wrist_R','LowerArm_L','LowerArm_R']:
 relative=ch.inv().apply(pos[b]-pos['Chest']);speed=np.linalg.norm(np.diff(relative,axis=0),axis=1)/dt
 i=int(speed.argmax())+1;speeds[b]={'max_cm_s':float(speed.max()),'p99_cm_s':float(np.percentile(speed,99)),'time':float(st[i]['time']),'case':int(st[i]['case'])}
out={'frames':n,'stock_mount_drift_during_full_aim_cm_max':float(np.linalg.norm(delta[aim],axis=1).max()),'stock_mount_drift_during_full_aim_cm_p95':float(np.percentile(np.linalg.norm(delta[aim],axis=1),95)),'fully_locked_foot_speed_cm_s_max':max(locked),'chest_relative_joint_speed':speeds,'visual_acceptance':False,'stock_scope':'rear buttpad relative to calibrated shoulder mount; not eye-line or cheek-contact acceptance'}
(d/'presentation-quality.json').write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2))
if a.require_refined:
 assert out['stock_mount_drift_during_full_aim_cm_max']<.02
 assert out['fully_locked_foot_speed_cm_s_max']<.01
