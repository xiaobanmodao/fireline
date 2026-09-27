"""Measured, constant jog contact registration on captured native ALS poses.

No source key or target bind/skin edits. Solve one chest-space translation for
both hands and the entire weapon, plus fixed elbow-plane registrations. Runtime
blends these by the original PoseGait curve, never the Shift button.
"""
from pathlib import Path
import csv,json,hashlib
import numpy as np
from scipy.spatial.transform import Rotation as R
from scipy.spatial import ConvexHull
from scipy.optimize import least_squares
from body_proportion_geometry import Surface,signed
from build_whole_carry_candidate import unit,between,transform,descendant
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'unreal/Fireline/Saved/MatureMotionResearch'
D=BASE/'LiveCarryProject/Saved/LiveDirectional/calibration-60';REF=BASE/'ReferenceProject/Saved/BodyProportionStudy'
T=json.loads((REF/'target.json').read_text());B={n:transform(t) for n,t in T['reference'].items()};parents=T['parents'];poses={}
for r in csv.DictReader((D/'poses.csv').open(encoding='utf-8-sig')):
 poses.setdefault(int(r['frame']),{})[r['bone']]=(np.array([float(r[k]) for k in ['x','y','z']]),R.from_quat([float(r[k]) for k in ['qx','qy','qz','qw']]),np.array([float(r[k]) for k in ['sx','sy','sz']]))
states=[{k:float(v) for k,v in r.items()} for r in csv.DictReader((D/'states.csv').open(encoding='utf-8-sig'))]
body=Surface(REF/'candidate');gun_all=Surface(REF/'gun');_,unique=np.unique(gun_all.v.round(5),axis=0,return_index=True);gun=gun_all.subset(unique)
armmask=np.array([n.startswith('DJ_') or n.startswith('LowerArm_') or (n.startswith('UpperArm_') and np.linalg.norm(v-B[n][0])>9) for n,v in zip(body.dom,body.v)])
ids=np.flatnonzero(armmask);_,unique=np.unique(body.v[ids].round(5),axis=0,return_index=True);arms=body.subset(ids[unique])
chosen=[i for i in range(0,len(poses),18) if states[i]['jog_weight']>.01];cache=[]
for i in chosen:
 p=poses[i];v=body.deform(p);cache.append((i,p,ConvexHull(v[np.isin(body.dom,['Chest','Torso'])]).equations,states[i]['jog_weight']))

def solve(x,p,w):
 n=dict(p);ch=p['Chest'][1]*B['Chest'][1].inv();delta=ch.apply(x[:3]*w);errs=[]
 for j,side in enumerate(['L','R']):
  u,e,h=['UpperArm_'+side,'LowerArm_'+side,'DJ_wrist_'+side];s,el,hand=[p[k][0] for k in [u,e,h]];t=hand+delta
  axis=unit(t-s);d=np.linalg.norm(t-s);a=np.linalg.norm(B[e][0]-B[u][0]);b=np.linalg.norm(B[h][0]-B[e][0]);along=(a*a-b*b+d*d)/(2*d);rad=np.sqrt(max(1e-8,a*a-along*along));pole=unit(el-s-axis*np.dot(el-s,axis));ne=s+axis*along+R.from_rotvec(axis*x[3+j]*w).apply(pole)*rad
  n[u]=(s,between(el-s,ne-s)*p[u][1],p[u][2]);hr=p[h][1]*B[h][1].inv();natural=hr.apply(unit(B[h][0]-B[e][0]));dq=between(natural,t-ne)*hr
  for bone in [e,'DJ_forearm_'+side]:n[bone]=(ne+dq.apply(B[bone][0]-B[e][0]),dq*B[bone][1],B[bone][2])
  for bone in p:
   if descendant(bone,h,parents):n[bone]=(p[bone][0]+delta,p[bone][1],p[bone][2])
  bend=np.degrees(np.arccos(np.clip(np.dot(natural,unit(t-ne)),-1,1)))
  errs.extend([max(0,bend-37)*2,max(0,1.5-(a+b-d))*10]);errs.extend((ne-el)*.04)
 for bone in p:
  if bone.startswith('M4_'):n[bone]=(p[bone][0]+delta,p[bone][1],p[bone][2])
 return n,errs

def obj(x):
 rows=[]
 for i,p,h,w in cache:
  n,err=solve(x,p,w);rows.extend(err)
  for surf in [arms,gun]:
   distances=signed(surf.deform(n),h);rows.extend(np.maximum(0,1.0-distances)*3)
 rows.extend(x[:3]*.4);rows.extend(x[3:]*3)
 return rows
initial=np.zeros(5);fit=least_squares(obj,initial,bounds=([-12,-12,-12,-.45,-.45],[12,12,12,.45,.45]),max_nfev=45,diff_step=1e-4)
result={'chest_translation_cm':fit.x[:3].tolist(),'elbow_plane_radians':fit.x[3:].tolist(),'fit_success':bool(fit.success),'evaluations':fit.nfev,'frames_fit':chosen,'source_capture_sha256':hashlib.sha256((D/'poses.csv').read_bytes()).hexdigest(),'target_reference_sha256':hashlib.sha256((REF/'target.json').read_bytes()).hexdigest(),'weight':'actual original ALS Rifle run node AlphaScaleBiasClamp.InterpolatedResult; compiled node _1 validated against PoseState.GaitRunningAmount','scope':'candidate jog registration; full native final-pose and triangle validation required; no visual acceptance'}
(BASE/'LiveCarryProject/Saved/LiveDirectional/jog-fit.json').write_text(json.dumps(result,indent=2));print(json.dumps({k:v for k,v in result.items() if k!='frames_fit'},indent=2),flush=True)
