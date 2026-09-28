"""Bounded elbow-plane clearance from exact triangle clipping; not runtime smoothing."""
import sys,json,numpy as np
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import calibrate_live_aim as c
fit=json.loads((c.D.parent/'surface-fit.json').read_text());base=np.array(fit['fit'])
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
for delta in [0]+[v for mag in np.arange(.005,.101,.005) for v in [mag,-mag]]:
 x=base.copy();x[6]+=delta;bad=0;maxarea=0;wrist=0
 for row in c.cache:
  pose,_,m=c.solve(x,row);wrist=max(wrist,max(v[0] for v in m));verts=c.arms.deform(pose);h=row[6].copy();h[:,3]-=.15;d=np.einsum("ij,kj->ik",verts,h[:,:3],optimize=False)+h[:,3];faces=np.flatnonzero(d[c.arms.tris].min(axis=1).max(axis=1)<0)
  for face in faces:
   poly=clip(list(verts[c.arms.tris[face]]),h);area=sum(np.linalg.norm(np.cross(poly[j]-poly[0],poly[j+1]-poly[0]))*.5 for j in range(1,len(poly)-1));maxarea=max(maxarea,area);bad+=area>1e-5
  if bad:break
 trials.append({'delta_radians':float(delta),'intersections_expanded_torso':int(bad),'max_area_cm2':float(maxarea),'wrist_max_deg':wrist});print(trials[-1],flush=True)
 if bad==0 and wrist<44.5:
  fit['fit']=x.tolist();fit['aim_registration']=x.tolist();fit['clearance_refinement']={'kind':'minimum sampled left elbow-plane displacement with exact triangle clipping','source_fit':'surface-fit.json','delta_radians':float(delta),'expanded_torso_cm':.15,'frames':len(c.cache),'trials':trials};fit['scope']='offline registration plus triangle clearance refinement; final native audits required';(c.D.parent/'clearance-fit.json').write_text(json.dumps(fit,indent=2));break
else:raise RuntimeError('No feasible bounded elbow-plane correction')
