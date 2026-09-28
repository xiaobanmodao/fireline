"""Extract barycentric witnesses of actual native torso intersections for calibration."""
import sys,csv,json,numpy as np
from pathlib import Path
from scipy.spatial import ConvexHull
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from body_proportion_geometry import Surface
root=ROOT/'unreal/Fireline/Saved/MatureMotionResearch';ref=root/'ReferenceProject/Saved/BodyProportionStudy';base=root/'LiveCarryProject/Saved/LiveAim';d=base/'refined-60';docs=ROOT/'unreal/Fireline/Docs/Validation/live-aim'
body=Surface(ref/'candidate');gun=Surface(ref/'gun');bind=json.loads((ref/'target.json').read_text())['reference'];mask=np.array([n.startswith(('DJ_','LowerArm_')) or (n.startswith('UpperArm_') and np.linalg.norm(v-bind[n]['p'])>9) for n,v in zip(body.dom,body.v)]);armtris=body.tris[mask[body.tris].all(axis=1)]
hits=json.loads((d/'triangle-intersections.json').read_text());chosen={r['frame'] for r in hits};poses={}
for row in csv.DictReader((d/'poses.csv').open(encoding='utf-8-sig')):
 i=int(row['frame'])
 if i not in chosen:continue
 poses.setdefault(i,{})[row['bone']]={'p':[float(row[k]) for k in 'xyz'],'q':[float(row[k]) for k in ['qx','qy','qz','qw']],'s':[float(row[k]) for k in ['sx','sy','sz']]}
def clip(poly,planes):
 for plane in planes:
  if not poly:return []
  new=[];prev=poly[-1];pd=np.dot(prev,plane[:3])+plane[3]
  for point in poly:
   z=np.dot(point,plane[:3])+plane[3]
   if (z<=0)!=(pd<=0):new.append(prev+(point-prev)*(pd/(pd-z)))
   if z<=0:new.append(point)
   prev,pd=point,z
  poly=new
 return poly
result=[]
for i in chosen:
 P=poses[i];v=body.deform(P);g=gun.deform(P);h=ConvexHull(v[np.isin(body.dom,['Chest','Torso'])]).equations
 for hit in [r for r in hits if r['frame']==i]:
  part,face=hit['part'],hit['face'];tri=(v[armtris[face]] if part==0 else g[gun.tris[face]]);poly=clip(list(tri),h);point=np.mean(poly,axis=0);bary=np.linalg.lstsq(np.vstack([tri.T,np.ones(3)]),np.r_[point,1],rcond=None)[0]
  result.append({'frame':i,'part':part,'face':face,'barycentric':bary.tolist()})
(docs/'clearance-witnesses.json').write_text(json.dumps(result,indent=2)+'\n')
p=docs/'calibration-inputs.json';j=json.loads(p.read_text());j['critical_frames']=sorted(set(j['critical_frames'])|chosen);j['seed_fit']=json.loads((base/'attachment-refined-fit.json').read_text())['fit'];p.write_text(json.dumps(j,indent=2)+'\n');print('witnesses',len(result),'frames',sorted(chosen))
