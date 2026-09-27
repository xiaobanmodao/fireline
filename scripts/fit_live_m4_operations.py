"""FAILED EXPERIMENT: reproduce a constant operation registration diagnosis.

The resulting fit FAILED reach/wrist/clearance gates. Never apply it as a repair.
The selected source hand/weapon/mechanical relationship is preserved. No bind,
source-key or skin changes. Final native playback remains a separate gate.
"""
from pathlib import Path
import csv,json,hashlib
import numpy as np
from scipy.spatial.transform import Rotation as R
from scipy.spatial import ConvexHull
from scipy.optimize import least_squares
from body_proportion_geometry import Surface,signed
from build_whole_carry_candidate import unit,between,transform,descendant,local,compose,mixq
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'unreal/Fireline/Saved/MatureMotionResearch';REF=BASE/'ReferenceProject/Saved/BodyProportionStudy';D=BASE/'LiveCarryProject/Saved/LiveM4Operations/calibration-60'
T=json.loads((REF/'target.json').read_text());B={n:transform(t) for n,t in T['reference'].items()};H={n:transform(t) for n,t in T['hold'].items()};names=T['names'];parents=T['parents'];mid=(H['DJ_wrist_L'][0]+H['DJ_wrist_R'][0])*.5
clips={k:[{n:transform(v) for n,v in f.items()} for f in c['frames']] for k,c in json.loads((BASE/'ReferenceProject/Saved/M4OperationSource/actions.json').read_text())['clips'].items()}
def lerp(a,b,w):return (a[0]+w*(b[0]-a[0]),mixq(a[1],b[1],w),a[2]+w*(b[2]-a[2]))
def blend(a,b,w):
 out={n:lerp(a[n],b[n],w) for n in names};gun='M4_body'
 for n in names:
  if (n.startswith('M4_') and n!=gun) or n in ['DJ_wrist_L','DJ_wrist_R']:out[n]=compose(lerp(local(a[n],a[gun]),local(b[n],b[gun]),w),out[gun])
  elif n.startswith('DJ_') and n not in ['DJ_forearm_L','DJ_forearm_R']:out[n]=compose(lerp(local(a[n],a[parents[n]]),local(b[n],b[parents[n]]),w),out[parents[n]])
 return out

def native(state):
 w=state['operation_weight'] if state['operation_weight']>0 else state['draw_weight'];key=('Reload' if state['empty_reload'] else 'TacticalReload') if state['operation_weight']>0 else 'Draw' if state['draw_action']==1 else 'Remove' if state['draw_action']==2 else 'Idle';time=state['reload_time'] if state['operation_weight']>0 else state['draw_time'];fs=clips[key];f=np.clip(time*60,0,len(fs)-1);i=int(f);s=blend(fs[i],fs[min(i+1,len(fs)-1)],f-i);return blend(H,s,w),w
poses={}
for r in csv.DictReader((D/'poses.csv').open(encoding='utf-8-sig')):poses.setdefault(int(r['frame']),{})[r['bone']]=(np.array([float(r[k]) for k in ['x','y','z']]),R.from_quat([float(r[k]) for k in ['qx','qy','qz','qw']]),np.array([float(r[k]) for k in ['sx','sy','sz']]))
states=[{k:float(v) for k,v in r.items()} for r in csv.DictReader((D/'states.csv').open(encoding='utf-8-sig'))]
body=Surface(REF/'candidate');gun_all=Surface(REF/'gun');_,unique=np.unique(gun_all.v.round(5),axis=0,return_index=True);gun=gun_all.subset(unique)
mask=np.array([n.startswith('DJ_') or n.startswith('LowerArm_') or (n.startswith('UpperArm_') and np.linalg.norm(v-B[n][0])>9) for n,v in zip(body.dom,body.v)]);ids=np.flatnonzero(mask);_,unique=np.unique(body.v[ids].round(5),axis=0,return_index=True);arms=body.subset(ids[unique])
cache=[]
for i in range(0,len(poses),6):
 if max(states[i]['operation_weight'],states[i]['draw_weight'])<.001:continue
 p=poses[i];v=body.deform(p);N,w=native(states[i]);cache.append((i,p,N,w,ConvexHull(v[np.isin(body.dom,['Chest','Torso'])]).equations))
def solve(x,p,source,w):
 out=dict(p);q0=p['M4_body'][1]*H['M4_body'][1].inv();c0=p['M4_body'][0]-q0.apply(H['M4_body'][0]-mid);ch=p['Chest'][1]*B['Chest'][1].inv();q=ch*R.from_rotvec(x[:3]*w)*ch.inv()*q0;c=c0+ch.apply(x[3:6]*w);errs=[];metrics=[]
 transfer=lambda n:(c+q.apply(source[n][0]-mid),q*source[n][1],source[n][2])
 for j,side in enumerate(['L','R']):
  u,e,h=['UpperArm_'+side,'LowerArm_'+side,'DJ_wrist_'+side];s=p[u][0];el=p[e][0];guide=el+q0.apply(source[e][0]-H[e][0]);hand=transfer(h);t=hand[0]
  axis=unit(t-s);d=np.linalg.norm(t-s);a=np.linalg.norm(B[e][0]-B[u][0]);b=np.linalg.norm(B[h][0]-B[e][0]);along=(a*a-b*b+d*d)/(2*d);rad=np.sqrt(max(1e-8,a*a-along*along));pole=unit(guide-s-axis*np.dot(guide-s,axis));ne=s+axis*along+R.from_rotvec(axis*x[6+j]*w).apply(pole)*rad
  out[u]=(s,between(el-s,ne-s)*p[u][1],p[u][2]);hr=hand[1]*B[h][1].inv();natural=hr.apply(unit(B[h][0]-B[e][0]));dq=between(natural,t-ne)*hr
  for bone in [e,'DJ_forearm_'+side]:out[bone]=(ne+dq.apply(B[bone][0]-B[e][0]),dq*B[bone][1],B[bone][2])
  for bone in names:
   if descendant(bone,h,parents):out[bone]=transfer(bone)
  bend=np.degrees(np.arccos(np.clip(np.dot(natural,unit(t-ne)),-1,1)));errs.extend([max(0,bend-38)*2,max(0,1.5-(a+b-d))*15]);metrics.append((bend,a+b-d))
 for bone in names:
  if bone.startswith('M4_'):out[bone]=transfer(bone)
 return out,errs,metrics

def obj(x):
 rows=[]
 for i,p,n,w,h in cache:
  pose,err,_=solve(x,p,n,w);rows.extend(err)
  for surface in [arms,gun]:rows.extend(np.maximum(0,.75-signed(surface.deform(pose),h))*3)
 rows.extend(x[:3]*5);rows.extend(x[3:6]*.2);rows.extend(x[6:]*2);return rows
if __name__=='__main__':
 print('fitting',len(cache),'measured frames',flush=True)
 fit=least_squares(obj,np.zeros(8),bounds=([-.7]*3+[-25]*3+[-1]*2,[.7]*3+[25]*3+[1]*2),max_nfev=45,diff_step=1e-4)
 m=np.array([solve(fit.x,p,n,w)[2] for i,p,n,w,h in cache]);pen=[]
 for i,p,n,w,h in cache:
  pose,_,_=solve(fit.x,p,n,w);pen.append(max(0,-min(signed(sf.deform(pose),h).min() for sf in [arms,gun])))
 result={'fit':fit.x.tolist(),'success':bool(fit.success),'evaluations':fit.nfev,'frames':len(cache),'wrist_max_deg':m[:,:,0].max(axis=0).tolist(),'reach_min_cm':m[:,:,1].min(axis=0).tolist(),'penetration_max_cm':max(pen),'target_sha256':hashlib.sha256((REF/'target.json').read_bytes()).hexdigest(),'source_sha256':hashlib.sha256((BASE/'ReferenceProject/Saved/M4OperationSource/actions.json').read_bytes()).hexdigest(),'scope':'fit diagnosis, not native or visual acceptance'}
 (D.parent/'operation-fit.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2),flush=True)
