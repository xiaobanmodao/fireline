"""Measured native clearance calibration; requires retained local trace data."""
import sys,csv,json,numpy as np
from pathlib import Path
from scipy.spatial import ConvexHull
from scipy.spatial.transform import Rotation as R
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from body_proportion_geometry import Surface
from build_whole_carry_candidate import between,unit
root=ROOT/'unreal/Fireline/Saved/MatureMotionResearch';ref=root/'ReferenceProject/Saved/BodyProportionStudy';base=root/'LiveCarryProject/Saved/LiveAim';t=json.loads((ref/'target.json').read_text());b=t['reference'];body=Surface(ref/'candidate');mask=np.array([n.startswith(('DJ_','LowerArm_')) or (n.startswith('UpperArm_') and np.linalg.norm(v-b[n]['p'])>9) for n,v in zip(body.dom,body.v)]);tris=body.tris[mask[body.tris].all(axis=1)];cache=[]
for fps,ids in [(30,{527,528}),(60,{1055,1056})]:
 d=base/f'delivery-{fps}';states=list(csv.DictReader((d/'states.csv').open(encoding='utf-8-sig')));p={};raw={}
 for dest,file in [(p,'poses.csv'),(raw,'raw-poses.csv')]:
  for row in csv.DictReader((d/file).open(encoding='utf-8-sig')):
   i=int(row['frame'])
   if i in ids:dest.setdefault(i,{})[row['bone']]={'p':np.array([float(row[k]) for k in 'xyz']),'q':np.array([float(row[k]) for k in ['qx','qy','qz','qw']]),'s':np.array([float(row[k]) for k in ['sx','sy','sz']])}
 for i in ids:cache.append((fps,i,p[i],raw[i],float(states[i]['aiming_weight'])))
def clip(poly,planes):
 for plane in planes:
  if not poly:return []
  new=[];prev=poly[-1];pd=np.dot(prev,plane[:3])+plane[3]
  for point in poly:
   d=np.dot(point,plane[:3])+plane[3]
   if (d<=0)!=(pd<=0):new.append(prev+(point-prev)*(pd/(pd-d)))
   if d<=0:new.append(point)
   prev,pd=point,d
  poly=new
 return poly
trials=[]
for delta in -np.arange(0,.401,.005):
 bad=0;wrist=0
 for fps,i,P,raw,w in cache:
  p=dict(P);u,e,hand='UpperArm_L','LowerArm_L','DJ_wrist_L';sh=P[u]['p'];el=P[e]['p'];end=P[hand]['p'];axis=unit(end-sh);center=sh+axis*np.dot(el-sh,axis);ne=center+R.from_rotvec(axis*delta*w).apply(el-center);hr=R.from_quat(P[hand]['q'])*R.from_quat(b[hand]['q']).inv();natural=hr.apply(unit(np.array(b[hand]['p'])-b[e]['p']));dq=between(natural,end-ne)*hr
  p[u]={**P[u],'q':(between(raw[e]['p']-raw[u]['p'],ne-sh)*R.from_quat(raw[u]['q'])).as_quat()}
  for n in [e,'DJ_forearm_L']:p[n]={'p':ne+dq.apply(np.array(b[n]['p'])-b[e]['p']),'q':(dq*R.from_quat(b[n]['q'])).as_quat(),'s':b[n]['s']}
  wrist=max(wrist,float(np.degrees(np.arccos(np.clip(np.dot(natural,unit(end-ne)),-1,1)))));v=body.deform(p);h=ConvexHull(v[np.isin(body.dom,['Chest','Torso'])]).equations;h[:,3]-=.15;ds=np.einsum('ij,kj->ik',v,h[:,:3],optimize=False)+h[:,3];faces=np.flatnonzero(ds[tris].min(axis=1).max(axis=1)<0)
  for f in faces:
   poly=clip(list(v[tris[f]]),h);area=sum(np.linalg.norm(np.cross(poly[j]-poly[0],poly[j+1]-poly[0]))*.5 for j in range(1,len(poly)-1));bad+=area>1e-5
 trials.append({'delta':float(delta),'intersections':int(bad),'wrist_max':wrist})
 if bad==0 and wrist<45:
  fit=json.loads((base/'lowfps-fit.json').read_text());fit['moving_aim_pole_delta']=[float(delta),0];fit['moving_clearance']={'driver':'original ALS PoseMoving * Aiming state weight','expanded_torso_cm':.15,'frames':[[x[0],x[1]] for x in cache],'trials':trials};(base/'moving-fit.json').write_text(json.dumps(fit,indent=2));print(trials[-1]);break
else:raise RuntimeError('No moving elbow-plane fit')
