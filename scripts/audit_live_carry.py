"""Actual native pose validation for the isolated live M4 study; not art acceptance."""
from pathlib import Path
import argparse,csv,gzip,json,sys
import numpy as np
from scipy.spatial.transform import Rotation as R
from scipy.spatial import ConvexHull
from body_proportion_geometry import Surface
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'unreal/Fireline/Saved/MatureMotionResearch';REF=BASE/'ReferenceProject/Saved'
p=argparse.ArgumentParser();p.add_argument('--fps',type=int,default=60);p.add_argument('--parity',action='store_true');a=p.parse_args();D=BASE/'LiveCarryProject/Saved/LiveCarry'/f'{"parity" if a.parity else "audit"}-{a.fps}'
poses={}
with (D/'poses.csv').open(encoding='utf-8-sig') as f:
 for row in csv.DictReader(f):poses.setdefault(int(row['frame']),{})[row['bone']]={'p':[float(row[k]) for k in ['x','y','z']],'q':[float(row[k]) for k in ['qx','qy','qz','qw']],'s':[float(row[k]) for k in ['sx','sy','sz']]}
if a.parity:
 expected=json.loads(gzip.decompress((REF/'BodyProportionReview/poses.json.gz').read_bytes()))['contact'];pe=qe=se=0
 assert len(poses)==len(expected),(len(poses),len(expected))
 for i,actual in poses.items():
  for n,t in actual.items():
   e=expected[i][n];pe=max(pe,float(np.linalg.norm(np.array(t['p'])-e['p'])));qe=max(qe,float(np.degrees((R.from_quat(t['q'])*R.from_quat(e['q']).inv()).magnitude())));se=max(se,float(abs(np.array(t['s'])-e['s']).max()))
 result={'frames':len(poses),'position_error_cm_max':pe,'rotation_error_degrees_max':qe,'scale_error_max':se,'scope':'C++ live adaptation versus prior Python registration on the same recorded source poses'}
 (D/'audit.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2));assert pe<.002 and qe<.002 and se<.0001;sys.exit(0)
with (D/'states.csv').open(encoding='utf-8-sig') as f:states=[{k:float(v) for k,v in r.items()} for r in csv.DictReader(f)]
assert len(poses)==len(states) and states[-1]['failures']==0
body=Surface(REF/'BodyProportionStudy/candidate');gun=Surface(REF/'BodyProportionStudy/gun');target=json.loads((REF/'BodyProportionStudy/target.json').read_text());bind=target['reference'];hold=target['hold'];soles=[];feet=[];lengths=[];grip=[];wrist_bends=[];penetration=[];frames=[]
armmask=np.array([n.startswith('DJ_') or n.startswith('LowerArm_') or (n.startswith('UpperArm_') and np.linalg.norm(v-bind[n]['p'])>9) for n,v in zip(body.dom,body.v)])
armtris=body.tris[armmask[body.tris].all(axis=1)];intersections=[];separation=[1e9,1e9]
def clipped_triangle(tri,planes):
 poly=list(tri)
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
for i,P in poses.items():
 state=states[i];V=body.deform(P);h=ConvexHull(V[np.isin(body.dom,['Chest','Torso'])]).equations
 depths=[]
 for part,(vertices,tris,mask) in enumerate([(V,armtris,armmask),(gun.deform(P),gun.tris,np.ones(len(gun.v),bool))]):
  dots=np.einsum('ij,kj->ik',vertices,h[:,:3],optimize=False)+h[:,3];dist=dots[mask].max(axis=1);depths.append(float(max(0,-dist.min())));separation[part]=min(separation[part],float(dist.min()))
  # At 60 Hz also reject an edge/triangle crossing the torso with all vertices
  # outside it. Other frame rates repeat the complete native-pose checks.
  if a.fps==60:
   candidates=np.flatnonzero(dots[tris].min(axis=1).max(axis=1)<0)
   for face in candidates:
    poly=clipped_triangle(vertices[tris[face]],h)
    area=sum(np.linalg.norm(np.cross(poly[j]-poly[0],poly[j+1]-poly[0]))*.5 for j in range(1,len(poly)-1))
    if area>1e-4:intersections.append({'frame':i,'part':part,'face':int(face),'area_cm2':float(area)})
 penetration.append(depths)
 soles.append([float(V[body.dom=='Foot_'+s,2].min()+state['body_z']) for s in ['L','R']])
 feet.append([[P['Foot_'+s]['p'][1]+state['body_x'],-P['Foot_'+s]['p'][0]+state['body_y'],P['Foot_'+s]['p'][2]+state['body_z']] for s in ['L','R']])
 wristrow=[]
 for side in ['L','R']:
  for A,B in [('UpperArm','LowerArm'),('LowerArm','DJ_wrist'),('UpperLeg','LowerLeg'),('LowerLeg','Foot')]:
   u,e=A+'_'+side,B+'_'+side;lengths.append(abs(np.linalg.norm(np.array(P[e]['p'])-P[u]['p'])-np.linalg.norm(np.array(bind[e]['p'])-bind[u]['p'])))
  w='DJ_wrist_'+side;e='LowerArm_'+side
  actual=R.from_quat(P['M4_body']['q']).inv().apply(np.array(P[w]['p'])-P['M4_body']['p']);desired=R.from_quat(hold['M4_body']['q']).inv().apply(np.array(hold[w]['p'])-hold['M4_body']['p']);grip.append(np.linalg.norm(actual-desired))
  fore=np.array(P[w]['p'])-P[e]['p'];natural=(R.from_quat(P[w]['q'])*R.from_quat(bind[w]['q']).inv()).apply(np.array(bind[w]['p'])-bind[e]['p']);wristrow.append(np.degrees(np.arccos(np.clip(np.dot(fore,natural)/np.linalg.norm(fore)/np.linalg.norm(natural),-1,1))))
 wrist_bends.append(wristrow);frames.append(P)
soles=np.array(soles);feet=np.array(feet);dt=np.array([s['dt'] for s in states]);capsule=np.array([[s[k] for k in ['actor_x','actor_y','actor_z']] for s in states]);offset=np.array([[s[k] for k in ['body_x','body_y','body_z']] for s in states])-capsule
speed=np.array([s['speed'] for s in states]);idle=speed<.01
step={n:float(np.linalg.norm(np.diff(np.array([p[n]['p'] for p in frames]),axis=0),axis=1).max()) for n in ['LowerArm_L','LowerArm_R','DJ_wrist_L','DJ_wrist_R','LowerLeg_L','LowerLeg_R']}
result={'frames':len(frames),'fps':a.fps,'failed_poses':int(states[-1]['failures']),'max_speed_cm_s':float(speed.max()),'single_capsule_backward_step_cm_min':float(np.diff(capsule[:,0]).min()),'visual_to_capsule_xy_offset_cm_max':float(abs(offset[:,:2]).max()),'limb_length_error_cm_max':float(max(lengths)),'receiver_relative_grip_error_cm_max':float(max(grip)),'sole_height_cm_min':soles.min(axis=0).tolist(),'lowest_sole_height_cm_max':float(soles.min(axis=1).max()),'wrist_bend_deg_range':[np.min(wrist_bends,axis=0).tolist(),np.max(wrist_bends,axis=0).tolist()],'vertex_chest_intrusion_cm_max':np.max(penetration,axis=0).tolist(),'joint_step_cm_max':step,'minimum_reach_margin_cm':min(s['reach_margin'] for s in states),'visual_acceptance':False,'scope':'flat floor, forward M4, sustained/short input and interrupted stops; no aim/reload/sideways/air/slide or universal art acceptance'}
locks=np.array([[s.get('left_lock',0),s.get('right_lock',0)] for s in states]);foot_velocity=np.linalg.norm(np.diff(feet[:,:,:2],axis=0),axis=2)/dt[1:,None]
locked=(locks[1:]>.99)&(locks[:-1]>.99)
result.update({'skinned_sole_correction_cm_max':max(s.get('sole_correction',0) for s in states),'fully_locked_ankle_xy_speed_cm_s_max':float(foot_velocity[locked].max()) if locked.any() else None,'surface_vertex_clearance_cm_min':separation,'triangle_clipping_frames':len(frames) if a.fps==60 else 0,'intersecting_torso_triangles':len(intersections) if a.fps==60 else None})
(D/'audit.json').write_text(json.dumps(result,indent=2));(D/'triangle-intersections.json').write_text(json.dumps(intersections,indent=2));print(json.dumps(result,indent=2))
assert max(lengths)<.002 and max(grip)<.002
assert abs(offset[:,:2]).max()<.001 and np.diff(capsule[:,0]).min()>-.001
assert soles.min()>-.1 and soles.min(axis=1).max()<1.2
assert np.max(penetration)<.05
assert np.max(wrist_bends)<45
assert not intersections
