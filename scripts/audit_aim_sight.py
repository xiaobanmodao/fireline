"""Audit actual native head/neck lengths, visor sight proxy and helmet clearance."""
from pathlib import Path
import argparse,csv,json
import numpy as np
from scipy.spatial.transform import Rotation as R
from scipy.spatial import ConvexHull
from body_proportion_geometry import Surface

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
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'unreal/Fireline/Saved/MatureMotionResearch';REF=BASE/'ReferenceProject/Saved/BodyProportionStudy'
ap=argparse.ArgumentParser();ap.add_argument('directory',type=Path);ap.add_argument('--diagnose',action='store_true');ap.add_argument('--step',type=int,default=1);args=ap.parse_args();d=args.directory
if args.step<1:ap.error('--step must be positive')
if args.step!=1 and not args.diagnose:ap.error('Sampled checks are diagnostic only; use --diagnose or --step 1')
reg=json.loads((ROOT/'unreal/Fireline/Docs/Validation/aim-sight-registration/registration.json').read_text());target=json.loads((REF/'target.json').read_text());bind=target['reference'];body=Surface(REF/'candidate');gun=Surface(REF/'gun');states=list(csv.DictReader((d/'states.csv').open(encoding='utf-8-sig')));poses={}
for r in csv.DictReader((d/'poses.csv').open(encoding='utf-8-sig')):poses.setdefault(int(r['frame']),{})[r['bone']]={'p':np.array([float(r[k]) for k in 'xyz']),'q':[float(r[k]) for k in ['qx','qy','qz','qw']],'s':[float(r[k]) for k in ['sx','sy','sz']]}
proxy=np.array(reg['eye_head_local_cm']);eye_errors=[];relief=[];length_errors=[];hits=[];minsep=1e9
sampled=list(poses.items())[::args.step]
for i,p in sampled:
 eye=p['Head']['p']+R.from_quat(p['Head']['q']).apply(proxy);rear=p['M4_rearsight']['p'];f=p['M4_frontsight']['p']-rear;f/=np.linalg.norm(f);along=np.dot(eye-rear,f)
 if float(states[i]['aiming_weight'])>.999:eye_errors.append(float(np.linalg.norm(eye-rear-f*along)));relief.append(float(-along))
 for child,parent in [('Neck','Chest'),('Head','Neck'),('UpperArm_L','Shoulder_L'),('UpperArm_R','Shoulder_R')]:length_errors.append(abs(np.linalg.norm(p[child]['p']-p[parent]['p'])-np.linalg.norm(np.array(bind[child]['p'])-bind[parent]['p'])))
 v=body.deform(p);h=ConvexHull(v[body.dom=='Head']).equations;g=gun.deform(p);dots=np.einsum('ij,kj->ik',g,h[:,:3])+h[:,3];minsep=min(minsep,float(dots.max(axis=1).min()));candidates=np.flatnonzero(dots[gun.tris].min(axis=1).max(axis=1)<0)
 for face in candidates:
  poly=clip(list(g[gun.tris[face]]),h);area=sum(np.linalg.norm(np.cross(poly[j]-poly[0],poly[j+1]-poly[0]))*.5 for j in range(1,len(poly)-1))
  if area>1e-4:hits.append({'frame':i,'time':float(states[i]['time']),'face':int(face),'area_cm2':float(area)})
out={'frames':len(poses),'sampled_frames':len(sampled),'sample_step':args.step,'full_aim_frames':len(eye_errors),'eye_proxy_sight_error_cm_max':max(eye_errors) if eye_errors else None,'eye_proxy_sight_error_cm_p95':float(np.percentile(eye_errors,95)) if eye_errors else None,'eye_proxy_relief_cm_range':[min(relief),max(relief)] if relief else None,'neck_clavicle_length_error_cm_max':float(max(length_errors)) if length_errors else None,'gun_helmet_intersecting_triangles':len(hits),'gun_helmet_vertex_clearance_cm_min':minsep if sampled else None,'visual_acceptance':False,'eye_scope':reg['eye_definition'],'helmet_scope':'Conservative convex hull of deformed Head-dominant vertices, not an exact concave mesh test. Triangle count is per-frame instances; signed half-space clearance is not Euclidean surface distance.'}
(d/'sight-quality.json').write_text(json.dumps(out,indent=2));(d/'helmet-intersections.json').write_text(json.dumps(hits,indent=2));print(json.dumps(out,indent=2))
if not args.diagnose:
 assert sampled and eye_errors,'No sampled/full-aim frames'
 assert max(length_errors)<.002
 assert max(eye_errors)<.1
 assert min(relief)>4
 assert not hits
