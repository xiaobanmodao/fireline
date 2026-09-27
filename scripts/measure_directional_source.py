"""Compare final target foot trajectories with the exact live source poses."""
from pathlib import Path
import csv,json,argparse
import numpy as np
from scipy.spatial.transform import Rotation as R
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'unreal/Fireline/Saved/MatureMotionResearch';p=argparse.ArgumentParser();p.add_argument('--fps',type=int,default=60);a=p.parse_args();D=BASE/f'LiveCarryProject/Saved/LiveDirectional/audit-{a.fps}'
cal=json.loads((BASE/'LiveCarryProject/Saved/LiveCarry/calibration.json').read_text());bind=cal['target']['reference'];sb=cal['source_reference'];scale=cal['leg_scale']
s=[{k:float(v) for k,v in r.items()} for r in csv.DictReader((D/'states.csv').open(encoding='utf-8-sig'))]
def read(file,bones):
 out={n:[] for n in bones}
 for r in csv.DictReader(file.open(encoding='utf-8-sig')):
  if r['bone'] in out:out[r['bone']].append([float(r[k]) for k in ['x','y','z']])
 return {n:np.array(v) for n,v in out.items()}
t=read(D/'poses.csv',[b+'_'+q for q in ['L','R'] for b in ['UpperLeg','LowerLeg','Foot']]);src=read(D/'source-poses.csv',[b+'_'+q for q in ['l','r'] for b in ['thigh','calf','foot']]);q=R.from_euler('z',[x['actor_yaw']-90 for x in s],degrees=True);loc=np.array([[x[k] for k in ['actor_x','actor_y','actor_z']] for x in s]);dt=np.diff([x['time'] for x in s]);r={'frames':len(s),'fps':a.fps,'source':'same live ALS graph output, not a separate authored clip','feet':{},'turns':{}}
for side,low,lock in [('L','l','left_lock'),('R','r','right_lock')]:
 expected=np.array(bind['UpperLeg_'+side]['p'])+(src['foot_'+low]-sb['thigh_'+low]['p'])*scale
 error=np.linalg.norm(expected[:,:2]-t['Foot_'+side][:,:2],axis=1)
 tw=q.apply(t['Foot_'+side])+loc;sw=q.apply(src['foot_'+low]*scale)+loc;mask=np.array([x[lock]>.99 for x in s]);mask=mask[1:]&mask[:-1]
 tv=np.linalg.norm(np.diff(tw[:,:2],axis=0),axis=1)/dt;sv=np.linalg.norm(np.diff(sw[:,:2],axis=0),axis=1)/dt
 knee={}
 for tag,points in [('target',[t[b+'_'+side] for b in ['UpperLeg','LowerLeg','Foot']]),('source',[src[b+'_'+low] for b in ['thigh','calf','foot']])]:
  hip,k,foot=points;axis=foot-hip;axis/=np.linalg.norm(axis,axis=1)[:,None];bend=k-hip-axis*np.einsum('ij,ij->i',k-hip,axis)[:,None];radius=np.linalg.norm(bend,axis=1);normal=bend/np.maximum(radius[:,None],1e-8);reliable=(radius[1:]>2)&(radius[:-1]>2);angles=np.degrees(np.arccos(np.clip(np.einsum('ij,ij->i',normal[1:],normal[:-1]),-1,1)))
  knee[tag]={'reliable_plane_step_degrees_max':float(angles[reliable].max()),'reliable_plane_flips_over_90':int((angles[reliable]>90).sum()),'minimum_bend_radius_cm':float(radius.min())}
 r['feet'][side]={'source_target_horizontal_error_cm_max':float(error.max()),'target_locked_ankle_speed_max':float(tv[mask].max()) if mask.any() else None,'source_scaled_locked_ankle_speed_max':float(sv[mask].max()) if mask.any() else None,'knees':knee}
for c in range(19,23):
 rows=[x for x in s if x['case']==c];end=rows[-1];r['turns'][str(c)]={'final_facing_error_deg':abs((end['actor_yaw']-end['view_yaw']+180)%360-180),'time_seconds':end['time']-rows[0]['time']}
(D/'source-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2))
assert all(v['source_target_horizontal_error_cm_max']<.002 for v in r['feet'].values())
assert all(v['knees']['target']['reliable_plane_flips_over_90']==0 for v in r['feet'].values())
assert all(v['final_facing_error_deg']<2 for v in r['turns'].values())
