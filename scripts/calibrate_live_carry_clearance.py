"""Fit a constant right-elbow clearance on the retained uncorrected live capture."""
import csv,json,sys,numpy as np
from pathlib import Path
from scipy.spatial.transform import Rotation as R
from scipy.spatial import ConvexHull
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'));from body_proportion_geometry import Surface
from build_whole_carry_candidate import between,unit
D=ROOT/'unreal/Fireline/Saved/MatureMotionResearch';ref=D/'ReferenceProject/Saved/BodyProportionStudy';out=D/'LiveCarryProject/Saved/LiveCarry';body=Surface(ref/'candidate');bind=json.loads((ref/'target.json').read_text())['reference'];poses={}
for row in csv.DictReader((out/'calibration-60/poses.csv').open()):poses.setdefault(int(row['frame']),{})[row['bone']]={'p':np.array([float(row[k]) for k in ['x','y','z']]),'q':[float(row[k]) for k in ['qx','qy','qz','qw']],'s':np.array([float(row[k]) for k in ['sx','sy','sz']])}
mask=np.array([n=='LowerArm_R' or n=='DJ_forearm_R' or (n=='UpperArm_R' and np.linalg.norm(v-bind[n]['p'])>9) for n,v in zip(body.dom,body.v)]);tri=body.tris[mask[body.tris].all(axis=1)];ids=np.unique(tri);lookup={j:i for i,j in enumerate(ids)};tri=np.array([[lookup[j] for j in t] for t in tri]);surface=body.subset(ids)
frames=sorted(set(range(0,len(poses),8))|set(range(124,139)));data=[]
for f in frames:
 p=poses[f];v=body.deform(p);h=ConvexHull(v[np.isin(body.dom,['Chest','Torso'])]).equations
 data.append((p,h))
def evaluate(delta):
 margin=1e6
 for p,h in data:
  p=dict(p);u,e,w='UpperArm_R','LowerArm_R','DJ_wrist_R';s=p[u]['p'];el=p[e]['p'];hand=p[w]['p'];ne=s+R.from_rotvec(unit(hand-s)*delta).apply(el-s)
  p[u]=dict(p[u],q=(between(el-s,ne-s)*R.from_quat(p[u]['q'])).as_quat())
  hr=R.from_quat(p[w]['q'])*R.from_quat(bind[w]['q']).inv();natural=hr.apply(unit(np.array(bind[w]['p'])-bind[e]['p']));dq=between(natural,hand-ne)*hr
  for n in [e,'DJ_forearm_R']:p[n]={'p':ne+dq.apply(np.array(bind[n]['p'])-bind[e]['p']),'q':(dq*R.from_quat(bind[n]['q'])).as_quat(),'s':np.array(bind[n]['s'])}
  v=surface.deform(p);dot=np.einsum('ij,kj->ik',v,h[:,:3],optimize=False)+h[:,3];margin=min(margin,float(dot[tri].min(axis=1).max(axis=1).min()))
 return margin
scan=[]
for degree in [0,2,4,6]:
 value=evaluate(np.radians(degree));scan.append([degree,value]);print(degree,value,flush=True)
valid=[x for x in scan if x[1]>.1];assert valid
end=min(valid,key=lambda x:abs(x[0]))[0];a,b=0,abs(end);sign=np.sign(end)
for i in range(10):
 mid=(a+b)/2
 if evaluate(np.radians(sign*mid))>=.1:b=mid
 else:a=mid
result={'right_elbow_pole_delta_degrees':float(sign*b),'right_elbow_pole_delta_radians':float(np.radians(sign*b)),'minimum_tested_triangle_plane_separation_cm':evaluate(np.radians(sign*b)),'frames_fit':frames,'scan':scan,'scope':'constant geometry clearance calibration only; preserve both hands, rifle assembly, source hinge and fixed limb lengths; requires full native replay checks'}
(out/'clearance-fit.json').write_text(json.dumps(result,indent=2));print(json.dumps({k:v for k,v in result.items() if k!='frames_fit'},indent=2))
